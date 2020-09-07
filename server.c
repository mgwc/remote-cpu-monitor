/* 
This code primarily comes from 
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

// External global variables and functions
extern double curr_avg_usage_time;
extern double max_avg_usage_time;
extern double hour_avg_usage_time[3600];
extern int num_readings;
extern pthread_mutex_t lock;
void* read_idle_time(void*);

// Internal function declarations
double array_avg();
void* user_quit(void*);

//int start_server(int PORT_NUMBER)
void* start_server(void* PORT_NUMBER)
{

    //printf("Entered start_server\n");
    uint16_t CASTED_PORT = *((uint16_t*) PORT_NUMBER);
    //printf("Entered start_server; CASTED_PORT = %i\n", CASTED_PORT);
    
      // structs to represent the server and client
      struct sockaddr_in server_addr,client_addr;    
      
      int sock; // socket descriptor

      // 1. socket: creates a socket descriptor that you later use to make other system calls
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	perror("Socket");
	exit(1);
      }
      int temp = 1;
      if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
	perror("Setsockopt");
	exit(1);
      }

      // configure the server
      //server_addr.sin_port = htons(PORT_NUMBER); // specify port number
      server_addr.sin_port = htons(CASTED_PORT); // specify port number
      server_addr.sin_family = AF_INET;         
      server_addr.sin_addr.s_addr = INADDR_ANY; 
      bzero(&(server_addr.sin_zero),8); 
      
      // 2. bind: use the socket and associate it with the port number
      if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
	perror("Unable to bind");
	exit(1);
      }

      // 3. listen: indicates that we want to listen to the port to which we bound; second arg is number of allowed connections
      if (listen(sock, 1) == -1) {
	perror("Listen");
	exit(1);
      }
          
      // once you get here, the server is set up and about to start listening
      // printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
      printf("\nServer configured to listen on port %d\n", CASTED_PORT);
      fflush(stdout);
     
    int count = 0; // count the number of pages requested (for debugging purposes)
    
    while(1) { // keep looping and accept additional incoming connections
        
      // 4. accept: wait here until we get a connection on that port
      int sin_size = sizeof(struct sockaddr_in);
      int fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
      if (fd != -1) {
         printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

        // buffer to read data into
        char request[1024];

        // 5. recv: read incoming message (request) into buffer
        int bytes_received = recv(fd,request,1024,0);
        // null-terminate the string
        request[bytes_received] = '\0';
        // print it to standard out
        printf("REQUEST:\n%s\n", request);

        count++; // increment counter for debugging purposes

        // Create container for message to send back 
        char* response = (char*)malloc(1024 * sizeof(char));
          
        // Check whether request was to load page or to get JSON data
        int request_type = strncmp(request, "GET /data", 9);
        
        if (request_type != 0) {           // Request to load page
          
            //sprintf(response, "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<html><p>It works!<br>count=%d</p></html>", count);
            sprintf(response, "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<html><script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js\"></script><p>");

            pthread_mutex_lock(&lock);

            // Add latest CPU usage percentage to message
            char response_additions_buf[512];
            strcat(response, "<p id=\"last_cpu_usage\">Current/last CPU usage: ");
            sprintf(response_additions_buf, "%f", curr_avg_usage_time);
            strcat(response, response_additions_buf);
            strcat(response, "</p>\n");

            // Add maximum CPU usage observed since program started to message
            strcat(response, "<p id=\"max_cpu_usage\">Maximum CPU usage in last hour: ");
            sprintf(response_additions_buf, "%f", max_avg_usage_time);
            strcat(response, response_additions_buf);
            strcat(response, "</p>\n");

            // Add average CPU usage over past hour, or since program started if it has been running less than 1 hr
            strcat(response, "<p id=\"avg_cpu_usage\">Average CPU usage over last hour: ");
            double hour_avg = array_avg();
            sprintf(response_additions_buf, "%f", hour_avg);
            strcat(response, response_additions_buf);
            strcat(response, "</p>\n");

            pthread_mutex_unlock(&lock);

            // Add auto-update button to HTML
            strcat(response, "<p><button type=\"button\" id=\"autoUpdate\" onclick=\"setInterval(handle, 1000)\">Auto-Update</button></p>");
            
            // Add JavaScript for sending JSON request, receiving response, and using response
            strcat(response, "<script>function handle() { $.getJSON(\"/data\", function (data, status) { var latest = data.latest; var max = data.max; var average = data.average; $('#avg_cpu_usage').html(\"Average CPU usage over last hour: \" + average); $('#max_cpu_usage').html(\"Maximum CPU usage in last hour: \" + max); $('#last_cpu_usage').html(\"Current/last CPU usage: \" + latest); } ); } </script>");

            strcat(response, "</html>");
        
        } else {                          // Request for JSON data
            pthread_mutex_lock(&lock);
            sprintf(response, "HTTP/1.1 200 OK\nContent-Type: application/json\n\n{\"latest\" : \"%f\", \"max\" : \"%f\", \"average\" : \"%f\"}", curr_avg_usage_time, max_avg_usage_time, array_avg());
            pthread_mutex_unlock(&lock);
        }

        //printf("RESPONSE:\n%s\n", response);

        // 6. send: send the outgoing message (response) over the socket
        // note that the second argument is a char*, and the third is the number of chars	
        send(fd, response, strlen(response), 0);

          free(response);

        // 7. close: close the connection
        close(fd);
        //printf("Server closed connection\n");
       }
    }

    // 8. close: close the socket
    close(sock);
    printf("Server shutting down\n");
  
    return 0;
} 

double array_avg() {

    int stop_point;
    
    if (num_readings < 3600) {
        stop_point = num_readings;
    } else {
        stop_point = 3600;
    }
    
    if (num_readings == 0) {
        printf("No readings in hour_avg_usage_time yet \n");
    }
    
    double avg = hour_avg_usage_time[0];
    int i;
    for (i = 1; i < stop_point; i++) {
        avg = ((avg * (i)) + hour_avg_usage_time[i]) / (i + 1);
    }
    
    return avg;
}

/*
 * Quit program when user enters 'q'
 */ 
void* user_quit(void* p) {
    char user_input[3];
    while(1) {
        fgets(user_input, 3, stdin);
        if (user_input[0] == 'q') {
            if (user_input[1] == '\n') {
                if (user_input[2] == '\0') {
                    //printf("Shutting down\n");
                    return (void*) 1;
                }
            }
        }
    }
}


int main(int argc, char *argv[])
{
    /*
  // check the number of arguments
  if (argc != 2) {
      printf("\nUsage: %s [port_number]\n", argv[0]);
      exit(-1);
  }

  int port_number = atoi(argv[1]);
  if (port_number <= 1024) {
    printf("\nPlease specify a port number greater than 1024\n");
    exit(-1);
  }*/
    
    int port_number = 3000; // hard-coded for use on Codio
    
    // Create one thread each for HTTP server, reading CPU statistics, and user input reader
    pthread_t usage_thread;
    pthread_t http_thread;
    pthread_t user_input;
    pthread_mutex_init(&lock, NULL);
    
    pthread_create(&user_input, NULL, &user_quit, NULL);
    pthread_create(&usage_thread, NULL, &read_idle_time, NULL);
    pthread_create(&http_thread, NULL, &start_server, (void*) (&port_number));
    
    void* user_quit_ret;
    pthread_join(user_input, &user_quit_ret);    // Join on user_input thread s/t program quits when user enters 'q'
    
    if ((int) user_quit_ret == 1) {
        return 0;
    }
    
    pthread_join(http_thread, NULL);
    pthread_join(usage_thread, NULL);
}

