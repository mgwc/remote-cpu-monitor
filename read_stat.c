#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>

double read_idle_time_helper(int);

double curr_avg_usage_time;
double max_avg_usage_time;
double hour_avg_usage_time[3600];
int hour_avg_idx;
int num_readings;
pthread_mutex_t lock;

void* read_idle_time(void* p) {
    
    // Open device file
    int fd = open("/proc/stat", O_RDONLY);
    if (fd == -1) {
        printf("Error opening /proc/stat");
        exit(-1);
    }
    
    // Configure file I/O settings
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, 9600);
    cfsetospeed(&options, 9600);
    tcsetattr(fd, TCSANOW, &options);
    
    hour_avg_idx = 0;
    num_readings = 0;
    
    // Repeatedly calculate average usage time and related metrics
    while (1) {
        
        // Get two idle time readings for all CPUs one second apart
        double first_idle_time_reading = read_idle_time_helper(fd);
        sleep(1);
        double second_idle_time_reading = read_idle_time_helper(fd);
    
        // Calculate average average usage time for all CPUs in last second
        pthread_mutex_lock(&lock);
        double aggregate_idle_time = second_idle_time_reading - first_idle_time_reading;
        double avg_idle_time = aggregate_idle_time / 4;
        curr_avg_usage_time = 100 - avg_idle_time;
        
        // Update metrics and print current average usage time
        if (curr_avg_usage_time > max_avg_usage_time) {
            max_avg_usage_time = curr_avg_usage_time;
        }
        
        hour_avg_usage_time[hour_avg_idx] = curr_avg_usage_time;
        hour_avg_idx = (hour_avg_idx + 1) % 3600;
        num_readings++;
        
        if (curr_avg_usage_time < 0) {
            printf("Average usage time = 0\n");
        } else if (curr_avg_usage_time > 100) {
            printf("Average usage time = 100\n");
        } else {
            printf("Average usage time = %f\n", curr_avg_usage_time);
        }
        pthread_mutex_unlock(&lock);
    }
}

double read_idle_time_helper(int fd) {
    
    int lseek_ret = lseek(fd, 0, 0);     // Reset pointer in /proc/stat file
    if (lseek_ret == -1) {
        printf("Error with lseek()\n");
        return -1;
    }
    
    char buf[256];
    int bytes_read = read(fd, buf, 256);
    buf[bytes_read] = '\0';
    
    char delim = ' ';
    strtok(buf, &delim);
    int i = 0;
    while (i < 3) {                      // Advance to fourth number, which tells us idle time for the CPU
        strtok(NULL, &delim);
        //printf("%s\n", token);
        i++;
    }
    
    double idle_time = strtol(strtok(NULL, &delim), NULL, 10);
    return idle_time;
}