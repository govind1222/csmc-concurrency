# csmc-concurrency 
This is a project to gain understanding of threads in a Linux environment. This is a variation of the sleeping barber problem. The CSMC is a mentoring center where students can go to receive help with their assignments. The lab has a coordinator and a specified number of tutors ready to help students. There is also a specified number of chairs in the waiting room. As students come in to the CSMC and start filling up chairs, the coordinator queues up students based on priority and notifies the tutors that there are students to be helped. 

This is implemented using pthreads and is to help gain a better understanding of concurrent programming in C. 

## Compilation
Compile the file using gcc
```bash
gcc csmc.c -o csmc -Wall -Werror -O -pthread
```

## Running the file
In order to run the file, you need to pass in command line arguments. The first argument is the number of students coming for help at the CSMC. The second argument is the number of tutors providing help. The third argument is the number of chairs available in the waiting room. Lastly, the fourth argument is the total number of times each student can receive help.

Run the file using the following command
```bash
./csmc <students> <tutors> <chairs> <help>
```

For example
```bash
./csmc 10 3 4 5
```