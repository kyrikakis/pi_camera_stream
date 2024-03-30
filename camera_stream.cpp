#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <opencv2/opencv.hpp>
#include <libcamera/libcamera.h>
#include <libcamera/camera.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>

libcamera::CameraManager camera_manager;
std::shared_ptr<libcamera::Camera> camera;
std::mutex request_lock;
struct buffer_info_t
{
    void *data;
    size_t size;
};
std::unordered_map<const libcamera::FrameBuffer *, buffer_info_t> buffer_info;
const libcamera::Size size(1024, 768);
cv::Mat image(768, 1024, CV_8UC3);

void signal_callback_handler(int signum)
{
    std::cout << "Caught signal " << signum << std::endl;
    camera->requestCompleted.disconnect();
    request_lock.lock();
    if (camera->stop())
        std::cerr << "failed to stop camera" << std::endl;
    request_lock.unlock();
    camera->release();
    camera_manager.stop();
    for (const auto &e : buffer_info)
        if (munmap(e.second.data, e.second.size) == -1)
            std::cerr << "munmap failed: " << std::strerror(errno) << std::endl;
    exit(signum);
}

void processFrame(libcamera::Request *request)
{
    request_lock.lock();
    // Assuming the frame format is Bayer, you might need to adjust based on your camera's format
    std::cout << "Frame processed" << std::endl;
    request_lock.unlock();
}

int main()
{
    // Register signal and signal handler
    signal(SIGINT, signal_callback_handler);
    libcamera::Stream *stream;
    std::shared_ptr<libcamera::FrameBufferAllocator> allocator;
    std::vector<std::unique_ptr<libcamera::Request>> requests;

    camera_manager.start();
    if (camera_manager.cameras().empty())
        throw std::runtime_error("no cameras available");
    camera = camera_manager.cameras().front();

    if (!camera)
        throw std::runtime_error("failed to find camera");

    if (camera->acquire())
        throw std::runtime_error("failed to acquire camera");

    // configure camera stream
    std::unique_ptr<libcamera::CameraConfiguration> cfg =
        camera->generateConfiguration({libcamera::StreamRole::VideoRecording});

    if (!cfg)
        throw std::runtime_error("failed to generate configuration");

    libcamera::StreamConfiguration &scfg = cfg->at(0);

    scfg.size = size;
    // store selected stream configuration
    const libcamera::StreamConfiguration selected_scfg = scfg;

    switch (cfg->validate())
    {
    case libcamera::CameraConfiguration::Valid:
        break;
    case libcamera::CameraConfiguration::Adjusted:
        std::cout << "stream configuration adjusted from \""
                  << selected_scfg.toString() << "\" to \"" << scfg.toString()
                  << "\"";
        break;
    case libcamera::CameraConfiguration::Invalid:
        throw std::runtime_error("failed to valid stream configurations");
        break;
    }

    if (camera->configure(cfg.get()) < 0)
        throw std::runtime_error("failed to configure streams");

    std::cout << "camera \"" << camera->id() << "\" configured with "
              << scfg.toString() << " stream";

    // allocate stream buffers and create one request per buffer
    stream = scfg.stream();

    allocator = std::make_shared<libcamera::FrameBufferAllocator>(camera);
    allocator->allocate(stream);
    try {
        for (const std::unique_ptr<libcamera::FrameBuffer> &buffer : allocator->buffers(stream))
        {
            std::unique_ptr<libcamera::Request> request = camera->createRequest();
            if (!request)
                throw std::runtime_error("Can't create request");

            // multiple planes of the same buffer use the same file descriptor
            size_t buffer_length = 0;
            int fd = -1;
            for (const libcamera::FrameBuffer::Plane &plane : buffer->planes())
            {
                if (plane.offset == libcamera::FrameBuffer::Plane::kInvalidOffset)
                    throw std::runtime_error("invalid offset");
                buffer_length = std::max<size_t>(buffer_length, plane.offset + plane.length);
                if (!plane.fd.isValid())
                    throw std::runtime_error("file descriptor is not valid");
                if (fd == -1)
                    fd = plane.fd.get();
                else if (fd != plane.fd.get())
                    throw std::runtime_error("plane file descriptors differ");
            }

            // memory-map the frame buffer planes
            void *data = mmap(nullptr, buffer_length, PROT_READ, MAP_SHARED, fd, 0);
            if (data == MAP_FAILED)
                throw std::runtime_error("mmap failed: " + std::string(std::strerror(errno)));
            buffer_info[buffer.get()] = {data, buffer_length};

            if (request->addBuffer(stream, buffer.get()) < 0)
                throw std::runtime_error("Can't set buffer for request");

            requests.push_back(std::move(request));
        }

        // register callback
        camera->requestCompleted.connect(processFrame);

        // start camera and queue all requests
        if (camera->start())
            throw std::runtime_error("failed to start camera");

        for (std::unique_ptr<libcamera::Request> &request : requests)
            camera->queueRequest(request.get());

    }
    catch (const std::runtime_error &e) {
      // ignore
      std::cerr << e.what();
    }
    // Set up the server socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8888); // Replace with the port you want to use

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        std::cerr << "Error binding socket" << std::endl;
        close(serverSocket);
        return -1;
    }

    if (listen(serverSocket, 1) == -1)
    {
        std::cerr << "Error listening on socket" << std::endl;
        close(serverSocket);
        return -1;
    }

    std::cout << "Waiting for a connection..." << std::endl;

    // Accept a connection
    int clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == -1)
    {
        std::cerr << "Error accepting connection" << std::endl;
        close(serverSocket);
        return -1;
    }

    std::cout << "Client connected!" << std::endl;

    while (true) {
    }

    return 0;
}
