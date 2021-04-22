#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

// maintains state for each student
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

// global variables needed for output
int available_chairs;
int num_students_helped = 0;
int num_requests = 0;
int num_students_receiving_help = 0;
int total_sessions_completed = 0;

// semaphores to notify coordinator and tutor that students are waiting
sem_t notify_coordinator;
sem_t notify_tutor;

// array of semaphores, one for each student - used to wake up student after tutoring
sem_t *semaphores;

pthread_mutex_t seats;
pthread_mutex_t priorityQueueLock;

// array of student and tutor pthreads
pthread_t *students;
pthread_t *tutors;
// only one pthread for coordinator
pthread_t coordinator;

// array of student structs, one for each student specified
struct student *studentPriorityStorage;

// shared data structure to maintain priority, tutor selects student
// with highest priority
int **priorityQueue;

// shared data structure to keep track of which student just arrived
// used by coordinator to place into priority queue
int *studentArrivedOrder;

// function that causes student thread to sleep to simulate programming
void simulate_programming() {
    // returns a random number between 0 and 2000 ms
    int time = rand() % 2001;
    usleep(time);
}

// function that causes thread to sleep to simulate tutoring
void simulate_tutoring() {
    // sleep for 0.2 ms
    // .2 ms * 1000 to convert it into microseconds
    usleep(200);
}

// terminates all the tutors
void terminate_tutors() {
    int i;
    for (i = 0; i < num_tutors; i++) {
        sem_post(&notify_tutor);
    }
}

void * student_function(void *studentStruct) {
    struct student *studentPriority = (struct student*)studentStruct;

    for (;;) {

        // checks to see if thread should terminate
        if(total_sessions_completed >= num_students * num_help || studentPriority->numHelped >= num_help) {
            sem_post(&notify_coordinator);
            pthread_exit(NULL);
        }

        // programs for a bit and then decides to go to the CSMC
        simulate_programming();

        // checks to see if there is a seat available
        pthread_mutex_lock(&seats);
        // if there are no seats available, go back to programming
        if(available_chairs <= 0) {
            printf("S: Student %d found no empty chair. Will try again later.\n", studentPriority->id);
            //printf("empty chairs: %d", available_chairs);
            pthread_mutex_unlock(&seats);
            continue;
        }

        available_chairs--;
        //printf("student %d decrementing", studentPriority->id);
        pthread_mutex_unlock(&seats);

        // enters this student into the data structure that keeps track of students
        // that just arrived to the CSMC
        pthread_mutex_lock(&priorityQueueLock);
        int i;
        for (i = 0; i < num_students * num_help; i++) {
            if(studentArrivedOrder[i] != -1) {
                continue;
            }
            studentArrivedOrder[i] = studentPriority->id;
            break;
        }
        printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentPriority->id, available_chairs);
        pthread_mutex_unlock(&priorityQueueLock);

        // notify coordinator
        sem_post(&notify_coordinator);

        // wait to be tutored
        sem_wait(&semaphores[studentPriority->id - 1]);

        // after student has been tutored, increment number of available seats
        pthread_mutex_lock(&seats);
        available_chairs++;
        //printf("student %d incrementing", studentPriorityStorage[student - 1].id);
        pthread_mutex_unlock(&seats);

        //after being tutored
        if(studentPriority->tutorId > 0) {
            printf("S: Student %d received help from Tutor %d.\n", studentPriority->id, studentPriority->tutorId);
        }
        // TODO : may need to put these in a lock to prevent rescheduling with same priority

        // tutor id reset, and priority goes down. number of times student has been helped
        // increases by one
        studentPriority->tutorId = -1;
        studentPriority->numHelped++;
        studentPriority->priority--;

    }
}

void * tutor_function(void* id) {
    int tutor = *(int *)id;
    int student;
    for (;;) {

        student = 0;

        if (total_sessions_completed >= num_students * num_help) {
            pthread_exit(NULL);
        }

        sem_wait(&notify_tutor);


        pthread_mutex_lock(&priorityQueueLock);
        int i = 0, j = 0;
        for (i = 0; i < num_help && !student; i++) {
            for (j = 0; j < num_students; j++) {
                if(priorityQueue[i][j] <= 0) {
                    continue;
                }
                student = priorityQueue[i][j];
                priorityQueue[i][j] = -2;
                break;
            }
        }
        pthread_mutex_unlock(&priorityQueueLock);

        // no student found
        if(student == 0) {
            continue;
        }

        total_sessions_completed++;


        num_students_receiving_help++;
        studentPriorityStorage[student - 1].tutorId = tutor;
        simulate_tutoring();

        sem_post(&semaphores[student - 1]);

        num_students_helped++;
        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d.\n", student, tutor, num_students_receiving_help, num_students_helped);

        num_students_receiving_help--;


    }
}

void enterIntoPriorityQueue(int i) {
    int student = studentArrivedOrder[i];
    int priority = num_help - studentPriorityStorage[student - 1].priority;
    int k;

    if(student <= 0 || studentPriorityStorage[student - 1].numHelped >= num_help) {
        return;
    }
    for (k = 0; k < num_students; k++) {
        if(priorityQueue[priority][k] != -1) {
            continue;
        }
        priorityQueue[priority][k] = student;
        printf("C: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d.\n",
               student, studentPriorityStorage[student - 1].priority, num_chairs - available_chairs, num_requests);
        //sem_post(&notify_tutor);
        break;
    }
}

void * coordinator_function(void) {

    //int student;
    for (;;) {
        //student = -1;
        if (total_sessions_completed >= num_help * num_students) {

            // terminate all tutors
            terminate_tutors();
            pthread_exit(NULL);
        }

        sem_wait(&notify_coordinator);
        num_requests++;

        pthread_mutex_lock(&priorityQueueLock);
        int i;
        for (i = 0; i < num_students * num_help; i++) {
            if(studentArrivedOrder[i] <= 0) {
                continue;
            }

            enterIntoPriorityQueue(i);
            studentArrivedOrder[i] = -2;
            sem_post(&notify_tutor);

        }


        pthread_mutex_unlock(&priorityQueueLock);




    }
}

int main(int argc, char *argv[]) {

    if(argc < 5) {
        return -1;
    }

    num_students = atoi(argv[1]);
    num_tutors   = atoi(argv[2]);
    num_chairs   = atoi(argv[3]);
    num_help     = atoi(argv[4]);

    if(num_students <= 0 || num_tutors <= 0 || num_chairs <= 0 || num_help <= 0) {
        return 0;
    }

    time_t t;
    srand(time(&t));

    available_chairs = num_chairs;

    // im not sure about the parameter list
    sem_init(&notify_coordinator, 0, 0);
    sem_init(&notify_tutor, 0, 0);
    //sem_init(&seats, 0, 1);

    pthread_mutex_init(&seats, NULL);
    pthread_mutex_init(&priorityQueueLock, NULL);

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

    studentArrivedOrder = (int *)malloc(num_help * num_students * sizeof(int));
    for (i = 0; i < num_help * num_students; i++) {
        studentArrivedOrder[i] = -1;
    }

    semaphores = (sem_t *)malloc(num_students * sizeof(sem_t));
    for (i = 0; i < num_students; i++) {
        sem_init(&semaphores[i], 0, 0);
    }



    if(pthread_create(&coordinator, NULL, (void *)coordinator_function, NULL)) {
        return -1;
    }


    // create num_student # of student threads and initialize a struct for each student to keep
    // track of its state
    for(i = 0; i < num_students; i++) {
        studentPriorityStorage[i].id = i + 1;
        studentPriorityStorage[i].priority = num_help;
        studentPriorityStorage[i].numHelped = 0;
        studentPriorityStorage[i].tutorId = -1;
        // making sure thread was created successfully
        if (pthread_create(&students[i], NULL, (void *)student_function, (void *)&studentPriorityStorage[i])) {
            // TODO ERROR
            return -1;
        }
    }

    for(i = 0; i < num_tutors; i++) {
        // making sure thread was created successfully
        int id = i + 1;
        if(pthread_create(&tutors[i], NULL, (void *)tutor_function, (void *)&id)) {
            // TODO ERROR
            return -1;
        }
    }

    // wait for all student threads to terminate
    for(i = 0; i < num_students; i++) {
        pthread_join(students[i], NULL);
    }

    // wait for all tutor threads to terminate
    for (i = 0; i < num_tutors; i++) {
        pthread_join(tutors[i], NULL);
    }

    // wait for coordinator thread to terminate
    pthread_join(coordinator, NULL);

    return 0;

}