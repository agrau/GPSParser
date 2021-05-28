#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <iostream>
#include <cstring>
#include <vector>
#include <ctime>
#include <time.h>
#include <math.h>

std::string sample_text = "$GPGGA,175320.00,4748.634872,N,01053.770287,E,1,08,0.9,717.1,M,47.0,M,,*65";

//---------------------------------------------------------------------------
// GPGGAParser
//---------------------------------------------------------------------------
class GPGGAParser {

private:
    //-----------------------------------------------------------------------
    // size will be always 15
    //-----------------------------------------------------------------------
    std::vector<std::string> _split_message;
    std::string _delimiter = ",";

    const double a = 6378137.0;                 // WGS-84 semi-major axis
    const double e2 = 6.6943799901377997e-3;    // WGS-84 first eccentricity squared

    double _latitude;
    double _longitude;
    double _altitude;
    int _timestamp;
    int _x;
    int _y;
    int _z;

private:
    //-----------------------------------------------------------------------
    // split message argument using the _delimiter
    //-----------------------------------------------------------------------
    void split(std::string message)
    {
        size_t last = 0;
        size_t next = 0;
        while ((next = message.find(_delimiter, last)) != std::string::npos) {
            _split_message.push_back(message.substr(last, next-last));
            last = next + _delimiter.length();
        }
       _split_message.push_back(message.substr(last));
    }

    //-----------------------------------------------------------------------
    // calculate
    //-----------------------------------------------------------------------
    void calculate()
    {
        //-------------------------------------------------------------------
        // calculate timestamp
        // nmea has only the hour, so the date that is used is the current
        //-------------------------------------------------------------------
        std::tm utc;

        time_t time_now = time(0);
        tm* now = localtime(&time_now);
        utc.tm_year = now->tm_year;
        utc.tm_mon = now->tm_mon;
        utc.tm_mday = now->tm_mday;

        std::string utc_from_nmea =  _split_message.at(1);
        utc.tm_hour = stoi(utc_from_nmea.substr(0, 2));
        utc.tm_min = stoi(utc_from_nmea.substr(2, 2));
        utc.tm_sec = stoi(utc_from_nmea.substr(4, 2));
        utc.tm_isdst = -1;
        _timestamp = timegm(&utc);

        //-------------------------------------------------------------------
        // the result values are in meters
        // the nmea value of altitude need to be in meters too
        // (see http://aprs.gids.nl/nmea/#gga)
        //-------------------------------------------------------------------
        std::string latitude_degress = _split_message.at(2).substr(0, 2);
        std::string latitude_minutes = _split_message.at(2).substr(2, _split_message.at(2).size());
        _latitude = std::stoi(latitude_degress) + std::stof(latitude_minutes) / 60;

        std::string longitude_degress = _split_message.at(4).substr(0, 2);
        std::string longitude_minutes = _split_message.at(4).substr(2, _split_message.at(4).size());
        _longitude = std::stoi(longitude_degress) + std::stof(longitude_minutes) / 60;

        _altitude = stof(_split_message.at(9));

        if (_split_message.at(3).compare("S") == 0) {
            _latitude *= -1.0;
        }
        if (_split_message.at(5).compare("W") == 0) {
            _longitude *= -1.0;
        }

        double n = a / sqrt(1 - e2 * sin(_latitude) * sin(_latitude));
        _x = (n + _altitude) * cos(_latitude) * cos(_longitude);
        _y = (n + _altitude) * cos(_latitude) * sin(_longitude);
        _z = (n * (1 - e2) + _altitude) * sin(_latitude);
    }

public:
    GPGGAParser() {}

    //-----------------------------------------------------------------------
    //
    //-----------------------------------------------------------------------
    void run(std::string message)
    {
        split(message);
        calculate();
    }

    void print_result()
    {
        std::cout << "latitude: " << _latitude << std::endl;
        std::cout << "longitude: " << _longitude << std::endl;
        std::cout << "altitude: " << _altitude << std::endl;
        std::cout << "x: " << _x << std::endl;
        std::cout << "y: " << _y << std::endl;
        std::cout << "z: " << _z << std::endl;
        std::cout << "timestamp: " << _timestamp << std::endl;
    }
};

//---------------------------------------------------------------------------
// error
//---------------------------------------------------------------------------
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

//---------------------------------------------------------------------------
// gps_parser
//---------------------------------------------------------------------------
void gps_parser(std::string gps_line)
{
    GPGGAParser *parser = new GPGGAParser();
    parser->run(gps_line);
    parser->print_result();
}

//---------------------------------------------------------------------------
// main
//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
       error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
             error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd,
                (struct sockaddr *) &cli_addr,
                &clilen);
    if (newsockfd < 0)
         error("ERROR on accept");

    while ( true )
    {
        bzero(buffer, 256);
        n = read(newsockfd, buffer, 255);
        if (n < 0)
            error("ERROR reading from socket");

        printf(" %s\n", buffer);
        std::string gps_line = buffer;
        if (gps_line.find("GPGGA") >= 0)
            gps_parser(gps_line);
    }

    close(newsockfd);
    close(sockfd);

    return 0;
}
