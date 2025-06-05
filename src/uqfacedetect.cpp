// uqfacedetect.cpp
// Face detection server (multithreaded), using OpenCV and custom protocol.

#include <iostream>
#include <thread>
#include <mutex>
#include <csignal>
#include <vector>
#include <atomic>
#include <semaphore.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "protocol.h"
#include <opencv2/opencv.hpp>


const std::string TMPFILE = "/tmp/imagefile.jpg";

// Global stats and synchronization
std::atomic<int> active_clients{0}, completed_clients{0};
std::atomic<int> detect_requests{0}, replace_requests{0}, invalid_requests{0};
sem_t file_sem, cascade_sem;

// Haar cascades (loaded once)
cv::CascadeClassifier face_cascade, eyes_cascade;

// Signal handling thread: waits for SIGHUP and prints stats
void sighup_thread_func() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    // Unblock SIGHUP in this thread only
    pthread_sigmask(SIG_UNBLOCK, &set, nullptr);

    while (true) {
        int sig;
        sigwait(&set, &sig);
        if (sig == SIGHUP) {
            // Print statistics to stderr as specified:
            std::cerr << "Clients connected: " << active_clients.load() << "\n"
                      << "Completed clients: " << completed_clients.load() << "\n"
                      << "Face detection requests: " << detect_requests.load() << "\n"
                      << "Face replace requests: " << replace_requests.load() << "\n"
                      << "Invalid requests: " << invalid_requests.load() << "\n";
            std::cerr.flush();
        }
    }
}

// Thread function to handle one client
void handle_client(int client_fd, uint32_t maxsize) {
    active_clients.fetch_add(1);
    char header[4], opcode;
    while (true) {
        // Read 4-byte prefix
        if (!recv_all(client_fd, header, 4)) {
            break; // EOF or error
        }
        uint32_t prefix = (uint32_t)(uint8_t)header[0] 
                        | (uint32_t)(uint8_t)header[1] << 8
                        | (uint32_t)(uint8_t)header[2] << 16
                        | (uint32_t)(uint8_t)header[3] << 24;
        if (prefix != PROTOCOL_PREFIX) {
            // Bad prefix: send response file contents (not using protocol) and exit:contentReference[oaicite:29]{index=29}
            invalid_requests.fetch_add(1);
            // In a real assignment environment, read the provided response file.
            // Here we simply close the connection.
            break;
        }
        // Read operation code
        if (!recv_all(client_fd, &opcode, 1)) break;
        if (opcode != OP_FACE_DETECT && opcode != OP_FACE_REPLACE) {
            // Invalid op
            std::string err = "invalid operation type";
            // Send error message
            uint32_t len = err.size();
            uint32_t prefix_le = PROTOCOL_PREFIX;
            // Prefix
            send_all(client_fd, (char*)&prefix_le, 4);
            // Op = error
            char op = OP_ERROR_MESSAGE;
            send_all(client_fd, &op, 1);
            // Length and message
            char lenbuf[4] = { (char)(len&0xFF), (char)(len>>8), (char)(len>>16), (char)(len>>24) };
            send_all(client_fd, lenbuf, 4);
            send_all(client_fd, err.c_str(), len);
            break;
        }
        bool isReplace = (opcode == OP_FACE_REPLACE);
        // Read size of first image (little-endian)
        char sizebuf[4];
        if (!recv_all(client_fd, sizebuf, 4)) break;
        uint32_t img1_size = (uint32_t)(uint8_t)sizebuf[0] 
                           | (uint32_t)(uint8_t)sizebuf[1] << 8
                           | (uint32_t)(uint8_t)sizebuf[2] << 16
                           | (uint32_t)(uint8_t)sizebuf[3] << 24;
        if (img1_size == 0) {
            std::string err = "image is 0 bytes";
            // send error (same protocol as above)
            uint32_t prefix_le = PROTOCOL_PREFIX;
            send_all(client_fd, (char*)&prefix_le, 4);
            char op = OP_ERROR_MESSAGE;
            send_all(client_fd, &op, 1);
            char lenbuf[4] = { (char)(err.size()&0xFF), (char)(err.size()>>8), (char)(err.size()>>16), (char)(err.size()>>24) };
            send_all(client_fd, lenbuf, 4);
            send_all(client_fd, err.c_str(), err.size());
            break;
        }
        if (maxsize != 0 && img1_size > maxsize) {
            std::string err = "image too large";
            uint32_t prefix_le = PROTOCOL_PREFIX;
            send_all(client_fd, (char*)&prefix_le, 4);
            char op = OP_ERROR_MESSAGE;
            send_all(client_fd, &op, 1);
            char lenbuf[4] = { (char)(err.size()&0xFF), (char)(err.size()>>8), (char)(err.size()>>16), (char)(err.size()>>24) };
            send_all(client_fd, lenbuf, 4);
            send_all(client_fd, err.c_str(), err.size());
            break;
        }
        // Receive image1 data
        std::vector<uchar> img1_data(img1_size);
        if (!recv_all(client_fd, (char*)img1_data.data(), img1_size)) break;

        std::vector<uchar> img2_data;
        uint32_t img2_size = 0;
        if (isReplace) {
            // Read second image size and data
            if (!recv_all(client_fd, sizebuf, 4)) break;
            img2_size = (uint32_t)(uint8_t)sizebuf[0] 
                      | (uint32_t)(uint8_t)sizebuf[1] << 8
                      | (uint32_t)(uint8_t)sizebuf[2] << 16
                      | (uint32_t)(uint8_t)sizebuf[3] << 24;
            if (img2_size == 0) {
                std::string err = "image is 0 bytes";
                uint32_t prefix_le = PROTOCOL_PREFIX;
                send_all(client_fd, (char*)&prefix_le, 4);
                char op = OP_ERROR_MESSAGE;
                send_all(client_fd, &op, 1);
                char lenbuf[4] = { (char)(err.size()&0xFF), (char)(err.size()>>8), (char)(err.size()>>16), (char)(err.size()>>24) };
                send_all(client_fd, lenbuf, 4);
                send_all(client_fd, err.c_str(), err.size());
                break;
            }
            if (maxsize != 0 && img2_size > maxsize) {
                std::string err = "image too large";
                uint32_t prefix_le = PROTOCOL_PREFIX;
                send_all(client_fd, (char*)&prefix_le, 4);
                char op = OP_ERROR_MESSAGE;
                send_all(client_fd, &op, 1);
                char lenbuf[4] = { (char)(err.size()&0xFF), (char)(err.size()>>8), (char)(err.size()>>16), (char)(err.size()>>24) };
                send_all(client_fd, lenbuf, 4);
                send_all(client_fd, err.c_str(), err.size());
                break;
            }
            img2_data.resize(img2_size);
            if (!recv_all(client_fd, (char*)img2_data.data(), img2_size)) break;
        }

        // Save first image to file (protect with file_sem):contentReference[oaicite:30]{index=30}
        sem_wait(&file_sem);
        {
            FILE *f = fopen(TMPFILE.c_str(), "wb");
            if (f) {
                fwrite(img1_data.data(), 1, img1_size, f);
                fclose(f);
            }
        }
        sem_post(&file_sem);

        // Load first image
        cv::Mat image1 = cv::imread(TMPFILE, cv::IMREAD_UNCHANGED);
        if (image1.empty()) {
            // Invalid image
            std::string err = "invalid image";
            uint32_t prefix_le = PROTOCOL_PREFIX;
            send_all(client_fd, (char*)&prefix_le, 4);
            char op = OP_ERROR_MESSAGE;
            send_all(client_fd, &op, 1);
            char lenbuf[4] = { (char)(err.size()&0xFF), (char)(err.size()>>8), (char)(err.size()>>16), (char)(err.size()>>24) };
            send_all(client_fd, lenbuf, 4);
            send_all(client_fd, err.c_str(), err.size());
            break;
        }

        std::vector<cv::Rect> faces;
        // Detect faces (thread-safe with cascade_sem):contentReference[oaicite:31]{index=31}
        sem_wait(&cascade_sem);
        face_cascade.detectMultiScale(image1, faces);
        sem_post(&cascade_sem);
        if (faces.empty()) {
            // No faces found
            std::string err = "no faces detected in image";
            uint32_t prefix_le = PROTOCOL_PREFIX;
            send_all(client_fd, (char*)&prefix_le, 4);
            char op = OP_ERROR_MESSAGE;
            send_all(client_fd, &op, 1);
            char lenbuf[4] = { (char)(err.size()&0xFF), (char)(err.size()>>8), (char)(err.size()>>16), (char)(err.size()>>24) };
            send_all(client_fd, lenbuf, 4);
            send_all(client_fd, err.c_str(), err.size());
            break;
        }

        if (!isReplace) {
            // Face detection: draw ellipses on faces and eyes:contentReference[oaicite:32]{index=32}
            sem_wait(&cascade_sem);
            // For each face, detect eyes and draw ellipses
            for (auto& face : faces) {
                // Draw ellipse around face
                cv::Point center(face.x + face.width/2, face.y + face.height/2);
                cv::ellipse(image1, center, cv::Size(face.width/2, face.height/2), 0, 0, 360, cv::Scalar(0,255,0), 2);
                // Detect eyes within face region
                cv::Mat faceROI = image1(face);
                std::vector<cv::Rect> eyes;
                eyes_cascade.detectMultiScale(faceROI, eyes);
                for (auto& eye : eyes) {
                    cv::Point ecenter(face.x + eye.x + eye.width/2, face.y + eye.y + eye.height/2);
                    int radius = cvRound((eye.width+eye.height)*0.25);
                    cv::ellipse(image1, ecenter, cv::Size(radius, radius), 0, 0, 360, cv::Scalar(255,0,0), 2);
                }
            }
            sem_post(&cascade_sem);
        } else {
            // Face replacement: overlay second image on each face
            cv::Mat image2;
            // Save second image to file and load it
            sem_wait(&file_sem);
            {
                FILE *f = fopen(TMPFILE.c_str(), "wb");
                if (f) {
                    fwrite(img2_data.data(), 1, img2_size, f);
                    fclose(f);
                }
            }
            sem_post(&file_sem);
            image2 = cv::imread(TMPFILE, cv::IMREAD_UNCHANGED);
            if (image2.empty()) {
                std::string err = "invalid image";
                uint32_t prefix_le = PROTOCOL_PREFIX;
                send_all(client_fd, (char*)&prefix_le, 4);
                char op = OP_ERROR_MESSAGE;
                send_all(client_fd, &op, 1);
                char lenbuf[4] = { (char)(err.size()&0xFF), (char)(err.size()>>8), (char)(err.size()>>16), (char)(err.size()>>24) };
                send_all(client_fd, lenbuf, 4);
                send_all(client_fd, err.c_str(), err.size());
                break;
            }
            // Overlay each face
            for (auto& face : faces) {
                cv::Mat resized;
                cv::resize(image2, resized, face.size());
                // Handle alpha channel if present
                for (int y = 0; y < face.height; ++y) {
                    for (int x = 0; x < face.width; ++x) {
                        cv::Vec4b pix = resized.at<cv::Vec4b>(y, x);
                        if (pix[3] > 0) { 
                            // alpha > 0, copy BGR
                            image1.at<cv::Vec3b>(face.y+y, face.x+x) = cv::Vec3b(pix[0], pix[1], pix[2]);
                        }
                    }
                }
            }
        }

        // Write output image to file (overwrite) and send to client
        sem_wait(&file_sem);
        cv::imwrite(TMPFILE, image1);  // save result
        sem_post(&file_sem);

        // Read output file into buffer
        std::vector<uchar> outbuf;
        {
            FILE *f = fopen(TMPFILE.c_str(), "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long outsz = ftell(f);
                fseek(f, 0, SEEK_SET);
                outbuf.resize(outsz);
                fread(outbuf.data(), 1, outsz, f);
                fclose(f);
            }
        }

        // Send protocol message with op=2 (output image)
        uint32_t prefix_le = PROTOCOL_PREFIX;
        send_all(client_fd, (char*)&prefix_le, 4);
        char op_out = OP_OUTPUT_IMAGE;
        send_all(client_fd, &op_out, 1);
        uint32_t outsz = outbuf.size();
        char outlen[4] = { (char)(outsz&0xFF), (char)(outsz>>8), (char)(outsz>>16), (char)(outsz>>24) };
        send_all(client_fd, outlen, 4);
        send_all(client_fd, (char*)outbuf.data(), outsz);

        // Increment request counters
        if (isReplace) replace_requests.fetch_add(1);
        else           detect_requests.fetch_add(1);
        // Then loop for next request
    }
    close(client_fd);
    active_clients.fetch_sub(1);
    completed_clients.fetch_add(1);
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: ./uqfacedetect connectionlimit maxsize [portnum]\n";
        return 20;
    }
    // Parse connectionlimit and maxsize
    int connectionlimit = atoi(argv[1]);
    uint32_t maxsize = (uint32_t)strtoul(argv[2], nullptr, 10);
    if (connectionlimit < 0 || connectionlimit > 10000) {
        std::cerr << "Usage: ./uqfacedetect connectionlimit maxsize [portnum]\n";
        return 20;
    }
    if ( (argv[2][0] == '+' || (argv[2][0] >= '0' && argv[2][0] <= '9')) == false ) {
        std::cerr << "Usage: ./uqfacedetect connectionlimit maxsize [portnum]\n";
        return 20;
    }
    std::string port_str = (argc == 4 ? argv[3] : "0");
    // Check port_str not empty
    if (port_str.empty()) {
        std::cerr << "Usage: ./uqfacedetect connectionlimit maxsize [portnum]\n";
        return 20;
    }
    // Test temporary file creation
    FILE *testf = fopen(TMPFILE.c_str(), "wb");
    if (!testf) {
        std::cerr << "uqfacedetect: unable to open the image file for writing\n";
        return 2;
    }
    fclose(testf);

    // Load Haar cascades
    std::string face_cascade_name = "/usr/share/opencv4/haarcascades/haarcascade_frontalface_alt2.xml";
    std::string eyes_cascade_name = "/usr/share/opencv4/haarcascades/haarcascade_eye_tree_eyeglasses.xml";
    if (!face_cascade.load(face_cascade_name) || !eyes_cascade.load(eyes_cascade_name)) {
        std::cerr << "uqfacedetect: unable to load a cascade classifier\n";
        return 16;
    }

    // Block SIGHUP in main thread, it will be handled by sighup_thread
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
    // Ignore SIGINT so Ctrl+C does not terminate the server
    std::signal(SIGINT, SIG_IGN);
    std::thread sighup_thread(sighup_thread_func);
    sighup_thread.detach();

    // Initialize semaphores
    sem_init(&file_sem, 0, 1);
    sem_init(&cascade_sem, 0, 1);

    // Setup TCP listening socket
    addrinfo hints = {}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo("127.0.0.1", port_str.c_str(), &hints, &res) != 0) {
        std::cerr << "uqfacedetect: unable to listen on given port \"" << port_str << "\"\n";
        return 3;
    }
    int listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listen_fd < 0) {
        std::cerr << "uqfacedetect: unable to listen on given port \"" << port_str << "\"\n";
        return 3;
    }
    // Allow reuse
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(listen_fd, res->ai_addr, res->ai_addrlen) != 0) {
        std::cerr << "uqfacedetect: unable to listen on given port \"" << port_str << "\"\n";
        return 3;
    }
    freeaddrinfo(res);
    if (listen(listen_fd, 5) != 0) {
        std::cerr << "uqfacedetect: unable to listen on given port \"" << port_str << "\"\n";
        return 3;
    }
    // Print the actual port number
    sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(listen_fd, (sockaddr*)&sin, &len) == 0) {
        int actual_port = ntohs(sin.sin_port);
        std::cerr << actual_port << std::endl;
        std::cerr.flush();
    }

    // Accept loop
    while (true) {
        sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) continue;
        // Connection limiting
        if (connectionlimit > 0 && active_clients.load() >= connectionlimit) {
            close(client_fd); // refuse extra clients
            continue;
        }
        // Spawn thread for client
        std::thread t(handle_client, client_fd, maxsize);
        t.detach();
    }
    // Cleanup (unreachable)
    close(listen_fd);
    return 0;
}