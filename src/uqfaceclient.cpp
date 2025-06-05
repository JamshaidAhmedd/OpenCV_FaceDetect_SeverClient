// uqfaceclient.cpp
// Face detection client: parses command-line, sends request, handles response.

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include "protocol.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./uqfaceclient portnum [--outputimage filename] [--replacefilename filename] [--detect filename]\n";
        return 18;
    }
    std::string port_str = argv[1];
    std::string infile1 = "";  // for --detect
    std::string infile2 = "";  // for --replacefilename
    std::string outfile = "";  // for --outputimage

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--outputimage") {
            if (!outfile.empty() || i+1 >= argc) {
                std::cerr << "Usage: ./uqfaceclient portnum [--outputimage filename] [--replacefilename filename] [--detect filename]\n";
                return 18;
            }
            outfile = argv[++i];
            if (outfile.empty()) { std::cerr << "Usage: ./uqfaceclient portnum ...\n"; return 18; }
        }
        else if (arg == "--replacefilename") {
            if (!infile2.empty() || i+1 >= argc) {
                std::cerr << "Usage: ./uqfaceclient portnum [--outputimage filename] [--replacefilename filename] [--detect filename]\n";
                return 18;
            }
            infile2 = argv[++i];
            if (infile2.empty()) { std::cerr << "Usage: ./uqfaceclient portnum ...\n"; return 18; }
        }
        else if (arg == "--detect") {
            if (!infile1.empty() || i+1 >= argc) {
                std::cerr << "Usage: ./uqfaceclient portnum [--outputimage filename] [--replacefilename filename] [--detect filename]\n";
                return 18;
            }
            infile1 = argv[++i];
            if (infile1.empty()) { std::cerr << "Usage: ./uqfaceclient portnum ...\n"; return 18; }
        }
        else {
            std::cerr << "Usage: ./uqfaceclient portnum [--outputimage filename] [--replacefilename filename] [--detect filename]\n";
            return 18;
        }
    }

    // Open input files if given
    std::vector<char> img1_data, img2_data;
    if (!infile1.empty()) {
        std::ifstream in1(infile1, std::ios::binary);
        if (!in1) {
            std::cerr << "uqfaceclient: cannot open the input file \"" << infile1 << "\" for reading\n";
            return 11;
        }
        img1_data.assign(std::istreambuf_iterator<char>(in1), {});
        in1.close();
    } else {
        // Read from stdin
        std::vector<char> buf(4096);
        while (true) {
            std::cin.read(buf.data(), buf.size());
            std::streamsize n = std::cin.gcount();
            if (n > 0) img1_data.insert(img1_data.end(), buf.data(), buf.data()+n);
            if (!std::cin || n == 0) break;
        }
    }
    if (!infile2.empty()) {
        std::ifstream in2(infile2, std::ios::binary);
        if (!in2) {
            std::cerr << "uqfaceclient: cannot open the input file \"" << infile2 << "\" for reading\n";
            return 11;
        }
        img2_data.assign(std::istreambuf_iterator<char>(in2), {});
        in2.close();
    }

    // Open output file if given
    std::ofstream out;
    if (!outfile.empty()) {
        out.open(outfile, std::ios::binary);
        if (!out) {
            std::cerr << "uqfaceclient: cannot open the output file \"" << outfile << "\" for writing\n";
            return 9;
        }
    }

    // Connect to server on localhost:port
    addrinfo hints = {}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("127.0.0.1", port_str.c_str(), &hints, &res) != 0) {
        std::cerr << "uqfaceclient: unable to connect to the server on port \"" << port_str << "\"\n";
        return 16;
    }
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        std::cerr << "uqfaceclient: unable to connect to the server on port \"" << port_str << "\"\n";
        return 16;
    }
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        std::cerr << "uqfaceclient: unable to connect to the server on port \"" << port_str << "\"\n";
        return 16;
    }
    freeaddrinfo(res);

    // Construct request
    uint32_t prefix_le = PROTOCOL_PREFIX;
    send_all(sockfd, (char*)&prefix_le, 4);
    char op = img2_data.empty() ? OP_FACE_DETECT : OP_FACE_REPLACE;
    send_all(sockfd, &op, 1);

    // Send image1
    uint32_t size1 = img1_data.size();
    char lenbuf[4] = { (char)(size1&0xFF), (char)(size1>>8), (char)(size1>>16), (char)(size1>>24) };
    send_all(sockfd, lenbuf, 4);
    if (size1 > 0) send_all(sockfd, img1_data.data(), size1);

    // If replace, send image2
    if (!img2_data.empty()) {
        uint32_t size2 = img2_data.size();
        char lenbuf2[4] = { (char)(size2&0xFF), (char)(size2>>8), (char)(size2>>16), (char)(size2>>24) };
        send_all(sockfd, lenbuf2, 4);
        send_all(sockfd, img2_data.data(), size2);
    }

    // Receive response
    char resp_prefix_buf[4];
    if (!recv_all(sockfd, resp_prefix_buf, 4)) {
        std::cerr << "uqfaceclient: a communication error occurred\n";
        return 10;
    }
    uint32_t resp_prefix = (uint32_t)(uint8_t)resp_prefix_buf[0] 
                        | (uint32_t)(uint8_t)resp_prefix_buf[1] << 8
                        | (uint32_t)(uint8_t)resp_prefix_buf[2] << 16
                        | (uint32_t)(uint8_t)resp_prefix_buf[3] << 24;
    if (resp_prefix != PROTOCOL_PREFIX) {
        std::cerr << "uqfaceclient: a communication error occurred\n";
        return 10;
    }
    char resp_op;
    if (!recv_all(sockfd, &resp_op, 1)) {
        std::cerr << "uqfaceclient: a communication error occurred\n";
        return 10;
    }
    // Read length
    char resp_lenbuf[4];
    if (!recv_all(sockfd, resp_lenbuf, 4)) {
        std::cerr << "uqfaceclient: a communication error occurred\n";
        return 10;
    }
    uint32_t resp_size = (uint32_t)(uint8_t)resp_lenbuf[0] 
                      | (uint32_t)(uint8_t)resp_lenbuf[1] << 8
                      | (uint32_t)(uint8_t)resp_lenbuf[2] << 16
                      | (uint32_t)(uint8_t)resp_lenbuf[3] << 24;
    // Read payload
    std::vector<char> resp_data(resp_size);
    if (resp_size > 0 && !recv_all(sockfd, resp_data.data(), resp_size)) {
        std::cerr << "uqfaceclient: a communication error occurred\n";
        return 10;
    }

    if (resp_op == OP_OUTPUT_IMAGE) {
        // Write image data to output
        if (!outfile.empty()) {
            out.write(resp_data.data(), resp_size);
            out.close();
        } else {
            // stdout (binary)
            std::cout.write(resp_data.data(), resp_size);
        }
        close(sockfd);
        return 0;
    }
    else if (resp_op == OP_ERROR_MESSAGE) {
        std::string msg(resp_data.data(), resp_size);
        std::cerr << "uqfaceclient: got the following error message: \"" << msg << "\"\n";
        close(sockfd);
        return 20;
    }
    else {
        // Unknown response
        std::cerr << "uqfaceclient: a communication error occurred\n";
        close(sockfd);
        return 10;
    }
    return 0;
}
