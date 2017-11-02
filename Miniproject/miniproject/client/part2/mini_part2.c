#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <ctype.h>


#include "mini_part2.h"

#define BUFSIZE 512
sem_t PID_sem, signal_sem;
bool stop = false;
double y = 0;
struct udp_conn connection;
pthread_mutex_t mutex;
volatile int period_us = 5000;


int udp_init_client(struct udp_conn *udp, int port, char *ip)
{
	struct hostent *host;

	if ((host = gethostbyname(ip)) == NULL) return -1;

	udp->client_len = sizeof(udp->client);
	// define servers
	memset((char *)&(udp->server), 0, sizeof(udp->server));
	udp->server.sin_family = AF_INET;
	udp->server.sin_port = htons(port);
	bcopy((char *)host->h_addr, (char *)&(udp->server).sin_addr.s_addr, host->h_length);

	// open socket
	if ((udp->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) return udp->sock;

	return 0;
}

int udp_send(struct udp_conn *udp, char *buf, int len)
{
	return sendto(udp->sock, buf, len, 0, (struct sockaddr *)&(udp->server), sizeof(udp->server));
}

int udp_receive(struct udp_conn *udp, char *buf, int len)
{
	int res = recvfrom(udp->sock, buf, len, 0, (struct sockaddr *)&(udp->client), &(udp->client_len));

	return res;
}

void udp_close(struct udp_conn *udp)
{
	close(udp->sock);
	return;
}

int clock_nanosleep(struct timespec *next)
{
	struct timespec now;
	struct timespec sleep;

	// get current time
	clock_gettime(CLOCK_REALTIME, &now);

	// find the time the function should sleep
	sleep.tv_sec = next->tv_sec - now.tv_sec;
	sleep.tv_nsec = next->tv_nsec - now.tv_nsec;

	// if the nanosecon is below zero, decrement the seconds
	if (sleep.tv_nsec < 0)
	{
		sleep.tv_nsec += 1000000000;
		sleep.tv_sec -= 1;
	}

	// sleep
	nanosleep(&sleep, NULL);

	return 0;
}

void timespec_add_us(struct timespec *t, long us)
{
	// add microseconds to timespecs nanosecond counter
	t->tv_nsec += us*1000;

	// if wrapping nanosecond counter, increment second counter
	if (t->tv_nsec > 1000000000)
	{
		t->tv_nsec -= 1000000000;
		t->tv_sec += 1;
	}
}

void* periodic_request()
{
	struct timespec next;
	clock_gettime(CLOCK_REALTIME, &next);

	while(1){
		if (stop){ break; }

		pthread_mutex_lock(&mutex);
		if (udp_send(&connection, "GET", strlen("GET") + 1) < 0){ perror("Error in sendto()"); }
		pthread_mutex_unlock(&mutex);

		timespec_add_us(&next, period_us);
		clock_nanosleep(&next);
	}
	pthread_mutex_unlock(&mutex);
	printf("Exit periodic request\n");
}


void* udp_listener()
{
	
	int bytes_received;
	char recvbuf[BUFSIZE];
	char testbuf[BUFSIZE];

	while(1){
		if (stop){ break; }

		bytes_received = udp_receive(&connection, recvbuf, BUFSIZE);

		if (bytes_received > 0){
			memcpy(testbuf, recvbuf, strlen("GET_ACK:"));
			if (testbuf[0] == 'G'){
				y = get_double(recvbuf);
				sem_post(&PID_sem);
			}
			if (testbuf[0] == 'S'){
				sem_post(&signal_sem);
			}
		}
	}
	sem_post(&signal_sem);
	sem_post(&PID_sem);
	printf("Exit udp listener\n");
}


void* PID_control()
{
	char sendbuf[64];
	double error = 0.0;
	double reference = 1.0;
	double integral = 0.0;
	double u = 0.0;
	int Kp = 10;
	int Ki = 800;
	double period_s = period_us/(1000.0*1000.0);

	while(1){
		if (stop){ break; }
		sem_wait(&PID_sem);

		error = reference - y;
		integral = integral + (error*period_s);
		u = Kp*error + Ki*integral;
		snprintf(sendbuf, sizeof sendbuf, "SET:%f", u);
		pthread_mutex_lock(&mutex);
		if (udp_send(&connection, sendbuf, strlen(sendbuf)+1) < 0){ perror("Error in sendto()"); }
		pthread_mutex_unlock(&mutex);
	}
	printf("Exit PID controller\n");
}


void* respond_to_server()
{
	while(1){
		if (stop){ break; }
		sem_wait(&signal_sem);
		pthread_mutex_lock(&mutex);
		if (udp_send(&connection, "SIGNAL_ACK", strlen("SIGNAL_ACK")+1) < 0){ perror("Error in sendto()"); }
		pthread_mutex_unlock(&mutex);
	}
	printf("Exit signal responder\n");
} 

double get_double(const char *str)
{
    /* First skip non-digit characters */
    /* Special case to handle negative numbers */
    while (*str && !(isdigit(*str) || ((*str == '-' || *str == '+') && isdigit(*(str + 1)))))
        str++;
 
    /* The parse to a double */
    return strtod(str, NULL);
}


int main(void){

	
	udp_init_client(&connection, 9999, "192.168.0.1");

	if (udp_send(&connection, "START", strlen("START")+1) < 0){ perror("Error in sendto()"); }
	
	if (pthread_mutex_init(&mutex, NULL) != 0)
	{
		perror("mutex initialization");
	}
	if (sem_init(&PID_sem, 0, 1) != 0)
	{
		perror("semaphore initialization");
	}
	if (sem_init(&signal_sem, 0, 1) != 0)
	{
		perror("semaphore initialization");
	}


	pthread_t udp_listener_thread, PID_control_thread, signal_responder_thread, periodic_request_thread;
	pthread_create(&udp_listener_thread, NULL, udp_listener, NULL);
	pthread_create(&PID_control_thread, NULL, PID_control, NULL);
	pthread_create(&signal_responder_thread, NULL, respond_to_server, NULL);
	pthread_create(&periodic_request_thread, NULL, periodic_request, NULL);

	usleep(500*1000);
	stop = true;
	if (udp_send(&connection, "STOP", strlen("STOP")) < 0){ perror("Error in sendto()"); }

	pthread_join(periodic_request_thread, NULL);
	pthread_join(udp_listener_thread, NULL);
	pthread_join(PID_control_thread, NULL);
	pthread_join(signal_responder_thread, NULL);
	
	pthread_mutex_destroy(&mutex);
	sem_destroy(&PID_sem);
	sem_destroy(&signal_sem);
	udp_close(&connection);

	return 0;
}