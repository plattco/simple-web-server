/*copyright 2020 plattcr 
 * A simple web-server.  
 * 
 * The web-server performs the following tasks:
 * 
 *     1. Accepts connection from a client.
 *     2. Processes cgi-bin GET request.
 *     3. If not cgi-bin, it responds with the specific file or a 404.
 * 
 */

#include <ext/stdio_filebuf.h>
#include <unistd.h>
#include <sys/wait.h>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

// Using namespaces to streamline code below
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

/** Forward declaration for method defined further below.  I use this url_decode method in your serveClient method.
 */
std::string url_decode(std::string data);

// Named-constants to keep pipe code readable below
const int READ = 0, WRITE = 1;

/**
 * splits a string into it's separate words and puts it into a vector
 * @param str
 * @return 
 */
std::vector<std::string> split(const std::string& str) {
    std::istringstream is(str);
    std::string word;
    std::vector<std::string> list;
    while (is >> std::quoted(word)) {
        list.push_back(word);
    }
    return list;
}

/**
 * needs to be used in order to pass CLAs into execvp 
 * copied from class slides and slightly modified
 * @param argList
 * @return 
 */
void childHelper(std::vector<std::string> argList) {  // same
        std::vector<char*> args;
for (int i = 0; i < argList.size(); i++) {
    args.push_back(&argList[i][0]);
}

args.push_back(nullptr);
// run the command via child process
execvp(args[0], &args[0]);
// command not found
std::cout << "Command " << argList[0] << " not found!\n";
// end 
exit(0);
}

/**
 * method to put together the string to display to the user using the data
 * returned by executing the requested commands
 * @param temp
 * @param temp2
 * @param pid
 * @param is
 * @param os
 */
void output(const std::string& temp, const std::string& temp2, int pid, 
            std::istream& is, std::ostream& os) { 
    // start with standard output sections
    os << "HTTP/1.1 " << temp2 << "\r\n" << "Content-Type: " << temp << "\r\n"
       << "Transfer-Encoding: chunked\r\n" << "Connection: Close\r\n\r\n";
    
    std::string line;
    while (std::getline(is, line)) {  // found in class slides
        // add \n to terminate each line
        line += "\n";  
        // adds the line size in hex (via discussion page)
        os << std::hex << line.size() << "r\n";
        // write the info into os for printing
        os << line << "\r\n";
    }   
    if (pid != -1) {  // case for errors
        int exit = 0;
        std::string tempError;
        waitpid(pid, &exit, 0);
        // found in canvas discussion to convert code to string
        tempError = "Exit code: " + std::to_string(exit); 
        // found syntax online
        // put size as hex and error info into os for printing
        os << std::hex << tempError.size() << "\r\n" << tempError << "\r\n";
    }
    // required last line for output
     os << "0\r\n\r\n";   
}

/**
 * method to execute the command given by the user and then calls the output
 * method to return the results of this to the user
 * @param cmd
 * @param args
 * @param os
 */
void exec(std::string cmd, std::string args, std::ostream& os) {
    // split the string into separate args
    std::vector<std::string> splitArgs = split(args);
    
    // insert command to beginning of args
    splitArgs.insert(splitArgs.begin(), cmd);
    // create int array that can receive data from childProcess
    // copied and modified from notes on pipes in c++
    int pipefd[2];
    pipe(pipefd);
    
    // use fork and execute to run commands properly
    // pipe code copied from online documentation on pipes in c++ and modified
    // to suit this program
    int pid = fork();
    if (pid == 0) {
        close(pipefd[READ]); 
        dup2(pipefd[WRITE], 1);  // dup creates a copy of file descriptor
        childHelper(splitArgs);
    } else {
        // close end of pipe 
        close(pipefd[WRITE]);
        // read inputs
        __gnu_cxx::stdio_filebuf<char> inOut(pipefd[READ], std::ios::in, 1);
        std::istream is(&inOut);
        // use output method to process childProcesses output
        std::string text = "text/plain";
        std::string text2 = "200 OK";
        output(text, text2, pid, is, os);
    }
    }

/**
 * gets the path from the given url. I created this method combining class slides and
 * online c++ documentation/discussions on strings
 * @param request
 * @return 
 */
std::string getPath(std::string& request) {
    int temp = request.find(' '); 
    int temp2 = request.rfind(' ');
    if (temp == std::string::npos || temp2 == std::string::npos) {
        // path not valid
        return "";
    }
    std::string filePath = request.substr(temp + 2, temp2 - temp - 2);
    return filePath;         
}

/**
 * Process HTTP request (from first line & headers) and
 * provide suitable HTTP response back to the client.
 * 
 * @param is The input stream to read data from client.
 * @param os The output stream to send data to client.
 */
void serveClient(std::istream& is, std::ostream& os) {
    std::string str, request, args, defPath, cmd, newArgs;
    int tempy;
    // read get request using getline
    std::getline(is, request);
    // bypass http stuffs
    while (std::getline(is, str) && (str != "\r") && str != "") {}
    
    // find request by looking for prefix
    args = getPath(request);  // path/request that user has passed via url
    // set default path as expected prefix
    defPath = "cgi-bin/exec?cmd=";  // the default we will use
    // set size of expected path
    const int defPathSize = defPath.size();  // size of default path
    
    if (request.find(defPath) == string::npos) {  // if arg path is expected
        // find command to run
       tempy = args.find("&args=", defPathSize);
       // use given url decode method
       cmd = url_decode(args.substr(defPathSize, 
                                                tempy - defPathSize));
       
       newArgs = url_decode(args.substr(tempy + 6));
       
       // run command using exec by passing the command to pass, the new args, 
       // and by using os for new data
       exec(cmd, newArgs, os);
    } else {
        // case for when URL input is not valid, pass incorrect args and use
        // this to print it as the incorrect path
    std::string error = "Invalid request: " + args;
    // push error to stream
    std::istringstream is(error);
    std::string basic1 = "text/plain";
    std::string basic2 = "404 Not Found";
    output(basic1, basic2, -1, is, os);
    }
}
// -----------------------------------------------------------
//       DO  NOT  ADD  OR MODIFY CODE BELOW THIS LINE
// -----------------------------------------------------------

/** Convenience method to decode HTML/URL encoded strings.

    This method must be used to decode query string parameters
    supplied along with GET request.  This method converts URL encoded
    entities in the from %nn (where 'n' is a hexadecimal digit) to
    corresponding ASCII characters.

    \param[in] str The string to be decoded.  If the string does not
    have any URL encoded characters then this original string is
    returned.  So it is always safe to call this method!

    \return The decoded string.
*/
std::string url_decode(std::string str) {
    // Decode entities in the from "%xx"
    size_t pos = 0;
    while ((pos = str.find_first_of("%+", pos)) != std::string::npos) {
        switch (str.at(pos)) {
            case '+': str.replace(pos, 1, " ");
            break;
            case '%': {
                std::string hex = str.substr(pos + 1, 2);
                char ascii = std::stoi(hex, nullptr, 16);
                str.replace(pos, 3, 1, ascii);
            }
        }
        pos++;
    }
    return str;
}

/**
 * Runs the program as a server that listens to incoming connections.
 * 
 * @param port The port number on which the server should listen.
 */
void runServer(int port) {
    io_service service;
    // Create end point
    tcp::endpoint myEndpoint(tcp::v4(), port);
    // Create a socket that accepts connections
    tcp::acceptor server(service, myEndpoint);
    std::cout << "Server is listening on port "
              << server.local_endpoint().port() << std::endl;
    // Process client connections one-by-one...forever
    while (true) {
        tcp::iostream client;
        // Wait for a client to connect
        server.accept(*client.rdbuf());
        // Process information from client.
        serveClient(client, client);
    }
}

/*
 * The main method that performs the basic task of accepting connections
 * from the user.
 */
int main(int argc, char** argv) {
    if (argc == 2) {
        // Process 1 request from specified file for functional testing
        std::ifstream input(argv[1]);
        serveClient(input, std::cout);
    } else {
        // Run the server on some available port number.
        runServer(0);
    }
    return 0;
}

// End of source code
