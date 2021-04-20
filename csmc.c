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
    int beingHelped;
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
int student_just_arrived = -1;

sem_t notify_coordinator;
sem_t notify_tutor;

pthread_mutex_t seats;
pthread_mutex_t priorityQueueLock;
pthread_mutex_t finishedTutoringLock;

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

    while (1) {
        if (studentPriority->numHelped >= num_help) {
            // exit the thread here
            sem_post(&notify_coordinator);
            pthread_exit(NULL);
        }

        simulate_programming();

        pthread_mutex_lock(&seats);
        if(available_chairs >= 1) {
            available_chairs--;
            num_requests++;
            //pthread_mutex_lock(&finishedTutoringLock);
            student_just_arrived = studentPriority->id;
            //pthread_mutex_unlock(&finishedTutoringLock);
            printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentPriority->id, available_chairs);
            // notifies coordinator that a student has arrived
            sem_post(&notify_coordinator);

            // being tutored here
            // printf("er\n");
            // while the number of times this student has been helped is equal to the old value
            // student is still being tutored -> while loop
            //while(studentPriority->beingHelped == 0);

            printf("S: Student %d recieved help from Tutor %d.\n", studentPriority->id, studentPriority->tutorId);

            studentPriority->priority--;
            studentPriority->numHelped++;
            studentPriority->tutorId = -1;
            pthread_mutex_lock(&finishedTutoringLock);
            studentPriority->beingHelped = 0;
            pthread_mutex_unlock(&finishedTutoringLock);


        }
        else {
            printf("S: Student %d found no empty chair. Will try again later.\n", studentPriority->id);
        }

        pthread_mutex_unlock(&seats);
    }
}

void tutor_function(void* id) {
    int tutorID = *(int *)id;
    int studentID = -1;

    while(1) {

        // total sessions that can be completed is == num_help * num_students
        if(total_sessions_completed == num_help * num_students) {
            // all threads finished
            pthread_exit(NULL);
        }

        // wait for coordinator to notify a tutor that a student
        // is waiting in the queue to recieve help
        sem_wait(&notify_tutor);

        pthread_mutex_lock(&priorityQueueLock);
        int i, j = 0;
        for (i = 0; i < num_help; i++) {
            for (j = 0; j < num_students; j++) {
                if(priorityQueue[i][j] > 0) {
                    studentID = priorityQueue[i][j];
                    // setting this to negative 2 instead of negative 1 to indicate that this spot had been taken before
                    // and lets coordinator know not to schedule someone at this spot in the future
                    priorityQueue[i][j] = -2;
                    i = num_help;
                    break;
                }
            }
        }

        // no student found in queue
        if(studentID <= 0) {
            pthread_mutex_unlock(&priorityQueueLock);
            continue;
        }

        // if execution reaches this point, then a student to tutor has been found

        num_students_receiving_help++;

        available_chairs++;

        pthread_mutex_unlock(&priorityQueueLock);

        simulate_tutoring();

        pthread_mutex_lock(&priorityQueueLock);
        total_sessions_completed++;

        studentPriorityStorage[studentID-1].beingHelped = 1;
        studentPriorityStorage[studentID-1].tutorId = tutorID;

        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d.\n",
               studentID, tutorID, num_students_receiving_help, total_sessions_completed);

        num_students_receiving_help--;
        studentID = -1;

        pthread_mutex_unlock(&priorityQueueLock);


    }
}

void coordinator_function(void) {

    int currentStudentId;
    while(1) {

        // total sessions that can be completed is == num_help * num_students
        if(total_sessions_completed >= num_help * num_students) {
            // all sessions finished - terminate tutors
            int i;
            for(i = 0; i < num_tutors; i++) {
                sem_post(&notify_tutor);
            }

            pthread_exit(NULL);
        }

        // coordinator waits for a student to notify that they are in line
        sem_wait(&notify_coordinator);

        pthread_mutex_lock(&priorityQueueLock);
        currentStudentId = student_just_arrived - 1;
        int priorityIndex = num_help - studentPriorityStorage[currentStudentId].priority;
        int i;
        for(i = 0; i < num_students; i++) {
            if(priorityIndex < num_help && priorityQueue[priorityIndex][i] == -1) {

                priorityQueue[priorityIndex][i] = studentPriorityStorage[currentStudentId].id;
                printf("C: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d.\n",
                       currentStudentId + 1, studentPriorityStorage[currentStudentId].priority, (num_chairs - available_chairs), num_requests);

                sem_post(&notify_tutor);
                student_just_arrived = -1;
                break;
            }
        }
        pthread_mutex_unlock(&priorityQueueLock);


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

    if(num_students <= 0 || num_tutors <= 0 || num_chairs <= 0 || num_help <= 0) {
        return 0;
    }

    available_chairs = num_chairs;
    available_tutors = num_tutors;

    // im not sure about the parameter list
    sem_init(&notify_coordinator, 0, 0);
    sem_init(&notify_tutor, 0, 0);

    pthread_mutex_init(&seats, NULL);
    pthread_mutex_init(&priorityQueueLock, NULL);
    pthread_mutex_init(&finishedTutoringLock, NULL);

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

    // create num_student # of student threads and initialize a struct for each student to keep
    // track of its state
    for(i = 0; i < num_students; i++) {
        studentPriorityStorage[i].id = i + 1;
        studentPriorityStorage[i].priority = num_help;
        studentPriorityStorage[i].numHelped = 0;
        studentPriorityStorage[i].tutorId = -1;
        studentPriorityStorage[i].beingHelped = 0;
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

    if(pthread_create(&coordinator, NULL, (void *)coordinator_function, NULL)) {
        return -1;
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