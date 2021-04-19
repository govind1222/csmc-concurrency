#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

struct student {
    int id;
    int priority;
    int numHelped;
    int tutorId;
};

// command line arguments
int num_students;
int num_tutors;
int num_chairs;
int num_help;

int available_chairs;
int available_tutors;
int num_students_helped = 0;
int num_requests = 0;
int num_students_receiving_help = 0;
int total_sessions_completed = 0;

sem_t notify_coordinator;
sem_t notify_tutor;

pthread_mutex_t seats;
//pthread_mutex_t tutors;

pthread_t *students;
pthread_t *tutors;
pthread_t coordinator;

struct student *studentPriorityStorage;

int **priorityQueue;

void simulate_programming() {
    // returns a random number between 0 and 2000 ms
    int time = rand() % 2001;
    usleep(time * 1000);
}

void simulate_tutoring() {
    // sleep for 0.2 s
    // 200 ms * 1000 to convert it into microseconds
    usleep(200 * 1000);
}

void student_function(void *studentStruct) {
    struct student *studentPriority = (struct student*)studentStruct;

    int oldNumHelped = 0;

    while (1) {

        oldNumHelped = studentPriority->numHelped;

        if (studentPriority->numHelped > num_help) {
            // exit the thread here
            sem_post(&notify_coordinator);
            pthread_exit(NULL);
        }

        simulate_programming();

        pthread_mutex_lock(&seats);

        if(available_chairs >= 1) {
            //studentPriority->numHelped++;

            available_chairs--;
            num_requests++;

            printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentPriority->id, available_chairs);
            // notifies coordinator that a student has arrived
            sem_post(&notify_coordinator);

            // being tutored here

            // while the number of times this student has been helped is equal to the old value
            // student is still being tutored -> while loop
            while(oldNumHelped == studentPriority->numHelped);

            printf("S: Student %d recieved help from Tutor %d.\n", studentPriority->id, studentPriority->tutorId);
            studentPriority->tutorId = -1;
        }
        else {
            printf("S: Student %d found no empty chair. Will try again later.\n", studentPriority->id);
        }

        pthread_mutex_lock(&seats);
    }
}

void tutor_function(void* id) {
    int tutorID = *(int *)id + 1;

    while(tutorID)
        ;
}

void coordinator_function(void) {

    while(1) {




    }

}

int main(int argc, char *argv[]) {


    if(argc != 5) {
        return -1;
    }

    num_students = atoi(argv[1]);
    num_tutors   = atoi(argv[2]);
    num_chairs   = atoi(argv[3]);
    num_help     = atoi(argv[4]);

    /*

        [
            B -> C -> D
            E -> F -> G -> A
        ]

    */


    if(num_students <= 0 || num_tutors <= 0 || num_chairs <= 0 || num_help <= 0) {
        return 0;
    }

    available_chairs = num_chairs;
    available_tutors = num_tutors;

    // im not sure about the parameter list
    sem_init(&notify_coordinator, 0, 0);
    sem_init(&notify_tutor, 0, 0);

    pthread_mutex_init(&seats, NULL);

    students = (pthread_t *)malloc(num_students * sizeof(pthread_t));
    tutors = (pthread_t *)malloc(num_tutors * sizeof(pthread_t));

    studentPriorityStorage = (struct student *)malloc(num_students * sizeof(struct student));

    int i;
    // initialize priority queue (matrix of ints)
    // the rows of the matrix represent priorities (i.e. row 0 stores students with highest priority)
    // students in each row are ordered according to arrival - basically a queue data structure
    // items with -1 value represents that there is no student in that spot
    priorityQueue = (int **)malloc(num_help * sizeof(int *));
    for(i = 0; i < num_help; i++) {
        priorityQueue[i] = (int *)malloc(num_students * sizeof(int));
        int j;
        for (j = 0; j < num_students; j++) {
            priorityQueue[i][j] = -1;
        }
    }


    time_t t;
    srand(time(&t));

    for(i = 0; i < num_students; i++) {
        studentPriorityStorage[i].id = i + 1;
        studentPriorityStorage[i].priority = num_help;
        studentPriorityStorage[i].numHelped = 0;
        studentPriorityStorage[i].tutorId = -1;
        if (pthread_create(&students[i], NULL, (void *)student_function, (void *)&studentPriorityStorage[i]))
        {
            // TODO ERROR
            return -1;
        }
    }

    for(i = 0; i < num_tutors; i++) {
        if(pthread_create(&tutors[i], NULL, (void *)tutor_function, (void *)&i)) {
            // TODO ERROR
            return -1;
        }
    }

    if(pthread_create(&coordinator, NULL, (void *)coordinator_function, NULL)) {
        return -1;
    }

    for(i = 0; i < num_students; i++) {
        pthread_join(students[i], NULL);
    }

    for (i = 0; i < num_tutors; i++) {
        pthread_join(tutors[i], NULL);
    }

    pthread_join(coordinator, NULL);

    return 0;

}