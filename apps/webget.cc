#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

using namespace std;

void get_URL( const string& host, const string& path )
{
  TCPSocket socket {};
  socket.connect( Address { host, "http" } );
  // telnet {HOST} http 连接到对应的服务端口

  const string request = "GET " + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n"
                         + "Connection: close\r\n\r\n";
  /*
  格式：
  GET {PATH} HTTP/1.1
  Host: {HOST}
  Connection: close
  */

  string_view remaining { request }; // 请求数据
  while ( not remaining.empty() ) {
    const size_t written = socket.write( remaining );
    remaining.remove_prefix( written );
  }

  string buffer;
  while ( true ) {
    socket.read( buffer );
    if ( buffer.empty() ) {
      if ( socket.eof() ) {
        break;
      }
      continue;
    }
    cout << buffer;
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
