#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

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
pthread_mutex_t studentArrivedLock;
// need a lock for this variable as it is what determines when
// each thread terminates
pthread_mutex_t totalSessionsLock;

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
    // returns a random number between 0 and 2000 us
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

// helper function that enters a particular student into the priority
// queue
void enterIntoPriorityQueue(int i) {
    int student = studentArrivedOrder[i];
    int priority = num_help - studentPriorityStorage[student - 1].priority;
    int k;
    // if invalid student or already recieved most # of help, then cannot enter - return
    if(student <= 0 || studentPriorityStorage[student - 1].numHelped >= num_help) {
        return;
    }
    // loop through priority queue and enter student into the appropriate place
    for (k = 0; k < num_students; k++) {
        if(priorityQueue[priority][k] != -1) {
            continue;
        }
        priorityQueue[priority][k] = student;
        printf("C: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d.\n",
               student, studentPriorityStorage[student - 1].priority, num_chairs - available_chairs, num_requests);
        break;
    }
}

// the function that the student threads will execute
void * student_function(void *studentStruct) {
    struct student *studentPriority = (struct student*)studentStruct;
    for (;;) {
        // checks to see if thread should terminate
        if(studentPriority->numHelped >= num_help) {
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
            /*int r;
            for(r = 0; r < num_students; r++) {
                printf("%d ", studentArrivedOrder[r]);
            }
            printf("new array\n");*/
            pthread_mutex_unlock(&seats);
            continue;
        }

        available_chairs--;
        //printf("student %d decrementing", studentPriority->id);
        pthread_mutex_unlock(&seats);

        // enters this student into the data structure that keeps track of students
        // that just arrived to the CSMC
        pthread_mutex_lock(&studentArrivedLock);
        int i;
        for (i = 0; i < num_students * num_help; i++) {
            if(studentArrivedOrder[i] != -1) {
                continue;
            }
            studentArrivedOrder[i] = studentPriority->id;
            break;
        }
        pthread_mutex_unlock(&studentArrivedLock);
        pthread_mutex_lock(&seats);
        printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentPriority->id, available_chairs);
        pthread_mutex_unlock(&seats);

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

// the function that the tutor threads will execute
void * tutor_function(void* id) {
    int tutor = *(int *)id;
    int student;
    //printf("%d\n", tutor);
    for (;;)
    {
        student = 0;

        // checks to see if there are anymore students that the tutor should wait for
        // if the total sessions completed is equal to num_students * num_help
        // no more students, exit
        pthread_mutex_lock(&totalSessionsLock);
        if (total_sessions_completed >= num_students * num_help) {
            pthread_mutex_unlock(&totalSessionsLock);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&totalSessionsLock);

        // wait for the coordinator to notify the tutor that there is someone
        // in the priority queue
        sem_wait(&notify_tutor);

        // loop through the priority queue, looking for the student with the highest
        // priority and proceeds to tutor that student
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
            // printf("found no student\n");
            continue;
        }

        // increment total_sessions
        pthread_mutex_lock(&totalSessionsLock);
        total_sessions_completed++;
        num_students_receiving_help++;
        pthread_mutex_unlock(&totalSessionsLock);

        // set the tutor id for this student so that it can print out the right values
        studentPriorityStorage[student - 1].tutorId = tutor;
        //studentPriorityStorage[student - 1].numHelped++;
        // sleep for a little bit to simulate tutoring
        simulate_tutoring();
        // notify student that tutoring has completed
        sem_post(&semaphores[student - 1]);
        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d.\n", student, tutor, num_students_receiving_help, total_sessions_completed);

        pthread_mutex_lock(&totalSessionsLock);
        num_students_receiving_help--;
        pthread_mutex_unlock(&totalSessionsLock);
    }
}

// this is the function that the coordinator thread runs
void coordinator_function(void) {

    for (;;) {
        // checks to see if all sessions have been completed
        // if it has, notifies all tutors to terminate
        // terminates itself
        //pthread_mutex_lock(&totalSessionsLock);
        if (total_sessions_completed >= num_help * num_students) {
            //pthread_mutex_unlock(&totalSessionsLock);
            // terminate all tutors
            terminate_tutors();
            pthread_exit(NULL);
        }
        //pthread_mutex_unlock(&totalSessionsLock);

        // waits to be notified that there is a student waiting
        sem_wait(&notify_coordinator);
        num_requests++;
        // loops through the array that maintains the order that each student arrived in
        // finds the students that have arrived and then puts them into the queue
        pthread_mutex_lock(&studentArrivedLock);
        int i;
        for (i = 0; i < num_students * num_help; i++) {
            if(studentArrivedOrder[i] <= 0) {
                continue;
            }

            pthread_mutex_lock(&priorityQueueLock);
            enterIntoPriorityQueue(i);
            studentArrivedOrder[i] = -2;
            pthread_mutex_unlock(&priorityQueueLock);
            sem_post(&notify_tutor);
        }
        pthread_mutex_unlock(&studentArrivedLock);

    }
}

int main(int argc, char *argv[]) {

    // makes sure that all 4 parameters have been passed in
    if(argc < 5) {
        return -1;
    }

    // saves input parameters into respective variables
    num_students = atoi(argv[1]);
    num_tutors   = atoi(argv[2]);
    num_chairs   = atoi(argv[3]);
    num_help     = atoi(argv[4]);

    // if any values are 0, terminates program
    // 0 students means no tutoring
    // 0 tutors - csmc cannot function without tutors
    // 0 chairs - no way for students to enter the queue
    // 0 help - if students cannot get help, no tutoring
    if(num_students <= 0 || num_tutors <= 0 || num_chairs <= 0 || num_help <= 0) {
        return 0;
    }

    // counter variable for all loops
    int i;
    // sets seed for random number generator
    time_t t;
    srand(time(&t));

    available_chairs = num_chairs;

    // initializes the semaphores - one for coordinator, one for tutor
    sem_init(&notify_coordinator, 0, 0);
    sem_init(&notify_tutor, 0, 0);
    // one semaphore for each student
    semaphores = (sem_t *)malloc(num_students * sizeof(sem_t));
    for (i = 0; i < num_students; i++) {
        sem_init(&semaphores[i], 0, 0);
    }

    // initializes the locks for seats as well as the shared priorityQueue structures
    pthread_mutex_init(&seats, NULL);
    pthread_mutex_init(&priorityQueueLock, NULL);
    pthread_mutex_init(&studentArrivedLock, NULL);
    pthread_mutex_init(&totalSessionsLock, NULL);

    // allocates memory for each student and tutor threads
    students = (pthread_t *)malloc(num_students * sizeof(pthread_t));
    tutors = (pthread_t *)malloc(num_tutors * sizeof(pthread_t));

    // allocates memory for the struct student array that maintains state for each student
    studentPriorityStorage = (struct student *)malloc(num_students * sizeof(struct student));

    // initialize priority queue (matrix of ints)
    // the rows of the matrix represent priorities (i.e. row 0 stores students with highest priority)
    // students in each row are ordered according to arrival - basically a queue data structure
    // items with -1 value represents that there is no student in that spot
    // items with -2 value represents a spot that has, at one point, been filled by a student
    priorityQueue = (int **)malloc(num_help * sizeof(int *));
    for(i = 0; i < num_help; i++) {
        priorityQueue[i] = (int *)malloc(num_students * sizeof(int));
        int j;
        for (j = 0; j < num_students; j++) {
            priorityQueue[i][j] = -1;
        }
    }

    // allocates memory for the array structure that keeps track of the order that students come in
    // a student can only come in as long as they can still ask for help so array of size num_help * num_students
    // is plenty of space
    // value of -1 represents an empty spot
    // value of -2 represents a spot that has, at one point, been filled by a student
    studentArrivedOrder = (int *)malloc(num_help * num_students * sizeof(int));
    for (i = 0; i < num_help * num_students; i++) {
        studentArrivedOrder[i] = -1;
    }

    // creates the coordinator thread, and makes sure it was successful
    if(pthread_create(&coordinator, NULL, (void *)coordinator_function, NULL)) {
        return -1;
    }

    // creates num_tutor # of tutor threads and gives them each an id
    for(i = 0; i < num_tutors; i++) {
        // making sure thread was created successfully
        int *pointer = (int *)malloc(sizeof(int));
        *pointer = i + 1;
        if(pthread_create(&tutors[i], NULL, (void *)tutor_function, (void *)pointer)) {
            return -1;
        }
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



    //printf("%d\n", total_sessions_completed);
    // after all threads have terminated, program can terminate
    return 0;
}