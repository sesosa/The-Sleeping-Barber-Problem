//
//  main.c
//  barberTest
//
//  This program simulates the sleeping barber problem using pthreads and mutices.
//  Rather than having one customer generation thread, I have a thread created for each customer.
//  Due to this, the random generator is used as wait time for customer arrival, rather than
//  probability of arrival. To make the simulation work, I used the sleep command to make
//  customers arrive at different times.
//
//  Created by Savannah Sosa on 9/26/15.
//  Copyright (c) 2015 Savannah Sosa. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define true 1
#define false 0

//struct to create queue to represent waiting room
typedef struct queue {
    int capacity;
    int size;
    int front;
    int rear;
    int *elements;
} queue;

//define mutices
pthread_mutex_t barberState;        //protects what action barber is performing
pthread_mutex_t waitingRoomSeats;   //only one customer can come through at a time
pthread_mutex_t barberChair;        //only one person can be in the barber chair at a time
pthread_mutex_t timer;              //only one action can add to timer at a time
pthread_mutex_t finished;           //customer is done at the barber shop

//define pthread conditions
pthread_cond_t barberSleeping;      //barber is available to give haircut
pthread_cond_t wakeUp;	            //wakes barber up
pthread_cond_t haircutCompleted;    //tells customer haircut is over

int allCustomersCompleted = false;  //barber is done for the day
int barberChairTaken = false;       //customer is currently being served
int visitCompleted = false;         //customer is done at barber shop

int numOfChairs;                    //number of waiting room chairs
int numOfCustomers;                 //number of customers that will come to the barber shop
int arrivalProbability;             //integer probability that a customer arrives during the simulation

int customersWhoGotHaircuts = 0;    //number of customers who got haircuts
int customerWaitTime = 0;           //wait time belonging to each customer
int averageWaitTime = 0;            //average wait time per customer
int maxWaitTime = 0;                //max wait time a customer experienced
int sleepTime = 0;                  //amount of time barber spent sleeping

queue *waitingRoom;                 //queue representing waiting room

queue *createWaitingRoom(int maxChairs);
int leaveWaitingRoom(queue *waitingRoom);
int getNextCustomer(queue *waitingRoom);
int enterWaitingRoom(queue *waitingRoom,int element);
void *customer(void *num);
void *barber(void *);
int randomInt();

//main method to create barber and customer threads and print output
int main(int argc, char **argv) {
    
    numOfChairs = atoi(argv[1]);
    numOfCustomers = atoi(argv[2]);
    arrivalProbability = atoi(argv[3]);

    waitingRoom = createWaitingRoom(numOfChairs); //create queue
    
    //initialize pthread conditions
    pthread_cond_init(&barberSleeping, NULL);
    pthread_cond_init(&wakeUp, NULL);
    pthread_cond_init(&haircutCompleted, NULL);
    
    //initialize mutices
    pthread_mutex_init(&barberState, NULL);
    pthread_mutex_init(&barberChair, NULL);
    pthread_mutex_init(&waitingRoomSeats, NULL);
    pthread_mutex_init(&finished, NULL);
    
    pthread_t btid; // ID for the barber thread
    pthread_t tid[numOfCustomers]; // IDs for customer threads
    
    //create barber thread
    pthread_create(&btid, 0, barber, 0);
    
    //create customer thread and attach an id to each
    int customer_ID[numOfCustomers]; // Customer IDs

    for (int i = 0; i < numOfCustomers; i++) {
        customer_ID[i] = i;
        pthread_create(&tid[i], 0, customer, &customer_ID[i]);
    }
    
    //wait for each thread to finish
    for(int i = 0; i < numOfCustomers; i++)
        pthread_join(tid[i], 0);
    
    allCustomersCompleted = true;
    pthread_cond_signal(&wakeUp ); // wake up barber
    pthread_join(btid, 0);
    
    averageWaitTime = averageWaitTime/customersWhoGotHaircuts;
    
    printf("\n\nNumber of customers that received haircuts: %i customers\n", customersWhoGotHaircuts);
    printf("Average wait time of customers: %i seconds\n", averageWaitTime);
    printf("Maximum time that any customer had to wait: %i seconds\n", maxWaitTime);
    printf("Amount of time that the barber spent sleeping: %i seconds\n", sleepTime);
}

//barber method that wakes barber and gives haircut
void *barber(void *arg) {
    while(!allCustomersCompleted) { // Customers remain to be serviced
        printf("barber: sleeping \n");
        pthread_mutex_lock(&barberState);
        pthread_cond_wait(&wakeUp, &barberState);
        pthread_mutex_unlock(&barberState);
        
        if(!allCustomersCompleted) {
            printf("barber: start haircut \n");
            customersWhoGotHaircuts++;
            customerWaitTime += 10;
            sleep(10); //simulate haircut
            printf("barber: finished haircut \n");
            pthread_cond_signal(&haircutCompleted); //tell customer haircut is over
        }
        else{
            printf("barber: done with all customers");
        }
    }
    return 0;
}

//customer method that takes in customer id and finds a spot in waiting room until barber is not busy
//if no chairs available the customer leaves without a haircut
void *customer(void *customerNumber) {
    int number = *(int *)customerNumber;
    int waitTime = randomInt(); // Simulate going to the barber shop
    if (waitTime <= arrivalProbability) { //if wait time is less than arrival probability
        while (waitTime > 10) {
            waitTime = waitTime / 10;     //use wait time
        }
    } else {
        waitTime = 5;                    //if it is too large use 5 as default
    }
    sleep(waitTime);
    printf("customer %d: arrive at the barber shop \n", number);
    
    //only one customer can enter at a time
    pthread_mutex_lock(&waitingRoomSeats);
    int checkWaitingRoom = enterWaitingRoom(waitingRoom, number);
    
    customerWaitTime++;
    
    if(checkWaitingRoom != 0) {
        
        pthread_mutex_unlock(&waitingRoomSeats);
        printf("customer %d: enter waiting room \n", number);
    
        //wait for barber to become free
        pthread_mutex_lock(&barberChair);
        if(barberChairTaken)
            pthread_cond_wait(&barberSleeping, &barberChair);
        customerWaitTime++;
        barberChairTaken = true;
        pthread_mutex_unlock(&barberChair);
    
        //open up a spot in the waiting room
        pthread_mutex_lock(&waitingRoomSeats);
        leaveWaitingRoom(waitingRoom);
        pthread_mutex_unlock(&waitingRoomSeats);
    
        //wake up barber
        pthread_cond_signal(&wakeUp);
    
        //wait until done with haircut
        printf("customer %d: getting haircut \n", number);
        pthread_mutex_lock(&finished);
        if(!visitCompleted)
            pthread_cond_wait(&haircutCompleted, &finished);
        visitCompleted = false; //reset for next customer
        pthread_mutex_unlock(&finished);
    
        //get out of barber chair
        pthread_mutex_lock(&barberChair);
        barberChairTaken = false;
        pthread_mutex_unlock(&barberChair);
        pthread_cond_signal(&barberSleeping);
        sleepTime++;
        printf("customer %d: going home \n", number);
        
        } else {
            pthread_mutex_unlock(&waitingRoomSeats);
            printf("customer %d: leaves without hair cut \n", number);
        }
    if (customerWaitTime > maxWaitTime)
        maxWaitTime = customerWaitTime;
    averageWaitTime += customerWaitTime;
    customerWaitTime = 0; //reset time
    
    return 0;
}

//returns a random integer
int randomInt() {
    int random = 1 + rand() % 100;
    return random;
}

//initializes the waiting room queue with the number of seats available
queue *createWaitingRoom(int maxChairs) {
    queue *waitingRoom;
    waitingRoom = (queue *)malloc(sizeof(queue));
    waitingRoom->elements = (int *)malloc(sizeof(int)*maxChairs);
    waitingRoom->size = 0;
    waitingRoom->capacity = maxChairs;
    waitingRoom->front = 0;
    waitingRoom->rear = -1;
    return waitingRoom;
}

//removes customer from waiting room when they are ready to get a haircut so there is space
//for a new customer to sit
int leaveWaitingRoom(queue *waitingRoom) {
    //if waiting room size is zero, there are no customers to be served
    if(waitingRoom->size == 0) {
        printf("waiting room is empty\n");
        return 0; //returns 0 if there are no more customers
    }
    else {
        waitingRoom->size--;
        waitingRoom->front++;
        if(waitingRoom->front == waitingRoom->capacity) {
            waitingRoom->front = 0;
        }
    }
    return 1; //returns 1 if customer was successfully removed
}

//gets the next customer to get a haircut
int getNextCustomer(queue *waitingRoom) {
    if(waitingRoom->size == 0) {
        printf("no customers in the waiting room\n");
        return 0; //returns 0 if no customers
    }
    //return the customer which is at the front
    return waitingRoom->elements[waitingRoom->front];
}

//adds a new customer to the waiting room queue
int enterWaitingRoom(queue *waitingRoom,int element) {
    //if the waiting room is full customer is not added
    if(waitingRoom->size == waitingRoom->capacity) {
        printf("waiting room is full\n");
        return 0; //returns 0 so customer knows to leave
    } else {
        waitingRoom->size++;
        waitingRoom->rear = waitingRoom->rear + 1;
        if(waitingRoom->rear == waitingRoom->capacity) {
            waitingRoom->rear = 0;
        }
        //adds customer to back of waiting room
        waitingRoom->elements[waitingRoom->rear] = element;
    }
    return 1;
}