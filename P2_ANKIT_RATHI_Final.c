/**
 * Header File Inclusion
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

/**
 * Preprocessor Directives
 */
#define MAX_NUMBER_OF_USERS 10
#define STATE0 0	//No Change State
#define STATE1 1 	//Handle State
#define STATE2 2	//Tweet Start Found
#define STATE3 3	//Follow Start Found
#define STATE4 4	//Read Found
#define STATE5 5	//Exit Found
#define PRINTDEBUGS 0

/**
 * Function Declarations
 */
void *thread_users_fn(void *data);
void *thread_tweeter_fn(void *data);
void *thread_streamer_fn(void *data);
int fnLineContains(char *input, const char *find);
void insert_into_Repository();
void print_Repository();
struct TweetNode* isFollowTagInRepository();

/**
 * Global Variable Declarations
 */
int iNoOfThreads = 0;
int iCurrentThreadInProgress = 0;
int iStreamerFinishedFlag = 0;
int iFollowCompleteFlag = 0;

/**
 * User File Token Data Structure
 */
struct UserFileToken
{
	char inputBuffer[140];
	char outputBuffer[140];
	char handleName[20];
	char oHandleName[20];
	char tweetTag[20];
	char followTag[20];
	int gStateOfUserThreads;
}UserFileToken;

struct UserFileToken userFileToken[MAX_NUMBER_OF_USERS];

/**
 * Tweet Repository Data Structure
 */
struct TweeterRepository
{
	char tweetTag[20];
	struct TweeterRepository *nextTag;
	struct TweetNode *nextTweet;
};

struct TweeterRepository *tr_head_node = NULL, *tr_temp_node = NULL;

/**
 * Tweet Data Structure inside Repository
 */
struct TweetNode
{
	char buffer[140];
	char handleName[20];
	struct TweetNode *next;
};

struct TweetNode *tweet_temp_node = NULL;

/**
 * Global Semaphores for resource sharing between Threads
 */
sem_t sem_user_to_streamer[MAX_NUMBER_OF_USERS];
sem_t sem_streamer_to_user[MAX_NUMBER_OF_USERS];
sem_t sem_streamer_to_tweeter;
sem_t sem_tweeter_to_streamer;
sem_t sem_s_to_t_follow;
sem_t sem_u_to_s_follow;
sem_t sem_t_to_s_output;
sem_t sem_s_to_u_output;


/**
 * Main Function
 */
int main(int argc, char **argv)
{
	int retValue = 0;
	int threadId[MAX_NUMBER_OF_USERS], threadId_streamer, threadId_tweeter;
	int i=0;
	pthread_t thread_id_users[MAX_NUMBER_OF_USERS], thread_id_tweeter, thread_id_streamer;
	
	/* 
	 * argc should be 2 for correct execution
	 */
	if(argc == 2)
    {
        /*
         * We print argv[0] assuming it is the program name
         */
        iNoOfThreads = strtol(argv[1], NULL, 10);
        if(iNoOfThreads > 10)
        {
			printf("Number of threads exceeds max possible(%d).\nProgram Terminating\n",MAX_NUMBER_OF_USERS);
			exit(-1);
		}
    }
    else
    {
		printf("Number of threads not supplied.\nProgram Terminating\n");
		exit(-1);
	}
	
	/* 
	 * Initialize all the semaphores with initial values of 0, as we are using them for signalling
	 */
	sem_init(&sem_streamer_to_tweeter, 0, 0);
	sem_init(&sem_tweeter_to_streamer, 0, 0);
	sem_init(&sem_s_to_t_follow, 0, 0);
	sem_init(&sem_u_to_s_follow, 0, 0);
	sem_init(&sem_t_to_s_output, 0, 0);
	sem_init(&sem_s_to_u_output, 0, 0);
	
	/* 
	 * 1. Initialize all the semaphores associated for each user,
	 * 2. Create n number of threads and pass the threadid as argument to thread.
	 */
	for(i=0;i<iNoOfThreads;i++)
	{
		sem_init(&sem_user_to_streamer[i], 0, 0);
		sem_init(&sem_streamer_to_user[i], 0, 0);
		userFileToken[i].gStateOfUserThreads = STATE0;
		
		threadId[i] = i;
		retValue = pthread_create(&thread_id_users[i], NULL, &thread_users_fn, &threadId[i]);
		if(retValue)
		{
			printf("ERROR; return code from pthread_create() is %d\n", retValue);
			exit(-1);
		}
	}
	threadId_tweeter = MAX_NUMBER_OF_USERS + 1;
	
	/* 
	 * Create the Tweeter Thread
	 */
	retValue = pthread_create(&thread_id_tweeter, NULL, &thread_tweeter_fn, &threadId_tweeter);
	if(retValue)
	{
		printf("ERROR; return code from pthread_create() is %d\n", retValue);
		exit(-1);
	}
	threadId_streamer = MAX_NUMBER_OF_USERS + 2;
	/* 
	 * Create the Streamer Thread
	 */
	retValue = pthread_create(&thread_id_streamer, NULL, &thread_streamer_fn, &threadId_streamer);
	if(retValue)
	{
		printf("ERROR; return code from pthread_create() is %d\n", retValue);
		exit(-1);
	}
	
	/*
	 * Main thread waits for all threads to execute before closing
	 */
	for(i=0;i<iNoOfThreads;i++)
	{
		pthread_join(thread_id_users[i], NULL);
	}
	pthread_join(thread_id_tweeter, NULL);
	pthread_join(thread_id_streamer, NULL);
	
	/*
	 * Destroy all the semaphores
	 */
	for(i=0;i<iNoOfThreads;i++)
	{
		sem_destroy(&sem_user_to_streamer[i]);
		sem_destroy(&sem_streamer_to_user[i]);
	}
	sem_destroy(&sem_streamer_to_tweeter);
	sem_destroy(&sem_tweeter_to_streamer);
	sem_destroy(&sem_s_to_t_follow);
	sem_destroy(&sem_u_to_s_follow);
	sem_destroy(&sem_t_to_s_output);
	sem_destroy(&sem_s_to_u_output);
	
	return 0;
}

/**
 * Function called by sender/transmitting threads to send data.
 */
void *thread_users_fn(void *data)
{
	int *tparams = (int*)data;
	int iThreadID = *tparams;
	
	/* 
	 * Declare the file pointer
	 * and open file and change the name of the file as per individual thread ids
	 */
	FILE *fp;
	char fileName[] = "useri.txt";
	fileName[4] = (char) (iThreadID + '0');
	/* open the file for reading */
	fp = fopen (fileName, "r");
	if(fp == NULL)
	{
		printf("Error while opening the file: %s\n", fileName);
		perror("Error while opening the file.\n");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		//Wait for signal from Streamer Thread for User Thread to continue.
		sem_wait(&sem_streamer_to_user[iThreadID]);
#if PRINTDEBUGS == 1
		printf("User%d: Signal Received: From=Streamer To=User%d\n", iThreadID, iThreadID);
#endif
		
		char *line = NULL;
		int ch;
		size_t len = 0;
	
		ch = getline(&line, &len, fp);

#if PRINTDEBUGS == 1
		printf("User%d: Line Found: %s", iThreadID, line);
#endif
		
		//Check if the line contains the 'Handle @'
		if(fnLineContains(line, "Handle @") == 1)
		{
			//Split the line and obtain the handle name and populate the data structure.
			char *token;
			char *delimiter = " ";
			token = strtok(line, delimiter);
			token = strtok(NULL, delimiter);
			strncpy(userFileToken[iThreadID].handleName, token, strlen(token)-1);
	
			//Update the state of the User Thread to STATE1
			userFileToken[iThreadID].gStateOfUserThreads = STATE1;
			
			sem_post(&sem_user_to_streamer[iThreadID]);

#if PRINTDEBUGS == 1
			printf("User%d: Signal Sent: From=User%d To=Streamer: State Update=%d\n", iThreadID, iThreadID, userFileToken[iThreadID].gStateOfUserThreads);
#endif
		}
		//Check if the line contains the 'Start #'
		else if(fnLineContains(line, "Start #") == 1)
		{
			//Split the line and obtain the start hashtag name and populate the data structure.
			char *token;
			char *delimiter = " ";
			token = strtok(line, delimiter);
			token = strtok(NULL, delimiter);
			strcpy(userFileToken[iThreadID].tweetTag, token);
			
			//printf("User%d: Add Tweet : %s",iThreadID, userFileToken[iThreadID].tweetTag);
			
			//Signal the Streamer Thread to continue with Other Users and start has been found.
			sem_post(&sem_user_to_streamer[iThreadID]);

#if PRINTDEBUGS == 1
			//printf("User%d: Signal Sent: From=User%d To=Streamer: Tweet Start tag detected\n", iThreadID, iThreadID);
#endif

#if PRINTDEBUGS == 1
			printf("User%d: Signal Sent: From=User%d To=Streamer: State Update=%d\n", iThreadID, iThreadID, userFileToken[iThreadID].gStateOfUserThreads);
#endif
			
			//Wait for Signal from Streamer Thread to Continue reading inside the start to end tag
			sem_wait(&sem_streamer_to_user[iThreadID]);
			
#if PRINTDEBUGS == 1
			printf("User%d: Signal Received: From=Streamer To=User%d: Read Next Line\n", iThreadID, iThreadID);
#endif

			int iLineCount = 1;
			while(1)
			{
				ch = getline(&line, &len, fp);
				//Check if the end# tag has been reached
				if(fnLineContains(line, "End #") == 1)
				{
					break;
				}
				
#if PRINTDEBUGS == 1
				printf("User%d: Get Line %d: %s", iThreadID, iLineCount, line);
#endif

				
				
				strncat(userFileToken[iThreadID].inputBuffer, line, strlen(line)-1);
				char *sp = " ";
				strcat(userFileToken[iThreadID].inputBuffer, sp);
				
				
#if PRINTDEBUGS == 1
				printf("User%d: Contents of Buffer:::%s\n",iThreadID, userFileToken[iThreadID].inputBuffer);
#endif
				
				//Signal the Streamer Thread to continue with other users.
				sem_post(&sem_user_to_streamer[iThreadID]);
				
#if PRINTDEBUGS == 1
				printf("User%d: Signal Sent: From=User%d To=Streamer: Line Read\n", iThreadID, iThreadID);
#endif
				
				//Wait for Signal from Streamer Thread to continue reading the necxt line between start and end tags.
				sem_wait(&sem_streamer_to_user[iThreadID]);
				
#if PRINTDEBUGS == 1
				printf("User%d: Signal Received: From=Streamer To=User%d: Read Next Line\n", iThreadID, iThreadID);
#endif				
				//printf("\tUser%d still tweeting...Adding to Buffer...\n", iThreadID);
				
				iLineCount++;
			}
			
			//Update the state of the User Thread to STATE2
			userFileToken[iThreadID].gStateOfUserThreads = STATE2;
			
			//Signal the Streamer that end has been encountered and it can now signal the tweeter thread to add the tweet into repository.
			sem_post(&sem_user_to_streamer[iThreadID]);
			
#if PRINTDEBUGS == 1
			printf("User%d: Signal Sent: From=User%d To=Streamer: Tweeting Completed\n", iThreadID, iThreadID);
#endif

			//Wait for the Signal from Streamer Thread regarding the sucess of the tweet getting added into the repository.
			sem_wait(&sem_streamer_to_user[iThreadID]);
#if PRINTDEBUGS == 1
			printf("User%d: Signal Received: From=Streamer To=User%d: Display the Message to User%d that tweet added sucessfully\n", iThreadID, iThreadID, iThreadID);
#else
			//printf("\tUser%d's tweet added sucessfully\n", iThreadID);
#endif
			
			//After the User Thread has received a signal from streamer that tweet has been added successfully, then
			//it can signal the Streamer that it can continue with other users.
			sem_post(&sem_user_to_streamer[iThreadID]);
			
			int iLoopIndex = 0;
			for(iLoopIndex = 0; iLoopIndex < 140; iLoopIndex++)
			{
				userFileToken[iThreadID].inputBuffer[iLoopIndex] = '\0';
			}
		}
		//Check if the line contains the 'Follow #'
		else if(fnLineContains(line, "Follow #") == 1)
		{
			//Split the line and obtain the handle name and populate the data structure.
			char *token;
			char *delimiter = " ";
			token = strtok(line, delimiter);
			token = strtok(NULL, delimiter);
			strcpy(userFileToken[iThreadID].followTag, token);
			
			//Update the state of the User Thread to STATE3
			userFileToken[iThreadID].gStateOfUserThreads = STATE3;
			
			printf("User%d: Follow : %s",iThreadID, userFileToken[iThreadID].followTag);
			
			iFollowCompleteFlag = 0;
			
			//Signal the Streamer Thread that state has been updated and it can follow the tweet the user wants to follow.
			sem_post(&sem_user_to_streamer[iThreadID]);
#if PRINTDEBUGS == 1
			printf("User%d: Signal Sent: From=User%d To=Streamer : State Update=%d\n",iThreadID, iThreadID, STATE3);
#endif
			
			while(1)
			{
				//User Thread Signals the Streamer Thread to start following the tweet the user wants to follow.
				sem_post(&sem_u_to_s_follow);
				
#if PRINTDEBUGS == 1
				printf("User%d: Signal Sent: From=User%d To=Streamer : Follow :%s\n",iThreadID, iThreadID, userFileToken[iThreadID].followTag);
#endif
				//Wait for the Signal from the Streamer for the output buffer to be populated
				sem_wait(&sem_s_to_u_output);

#if PRINTDEBUGS == 1
				printf("User%d: Signal Recevied: From=Streamer To=User%d : Output\n",iThreadID, iThreadID);
#endif

#if PRINTDEBUGS == 1
				printf("User%d::::::%s\n",iThreadID, userFileToken[iThreadID].outputBuffer);
#endif			
				//Check if the end of the follow tweet from the repository has been reached.
				if(1 == iFollowCompleteFlag)
				{
					break;
				}
				else
				{
					printf("\t%s::%s\n", userFileToken[iThreadID].oHandleName, userFileToken[iThreadID].outputBuffer);
				}
			}
			
			//Wait for the Signal from the Streamer Thread
			sem_wait(&sem_streamer_to_user[iThreadID]);

#if PRINTDEBUGS == 1
			printf("User%d: Signal Received: From=Streamer To=User%d: Follow Completed\n",iThreadID, iThreadID);
#endif
			//Signal the Streamer Thread that it can continue with other users.
			sem_post(&sem_user_to_streamer[iThreadID]);
		}
		//Check if the line contains the 'Read'
		else if(fnLineContains(line, "Read") == 1)
		{
			
			printf("User%d: Read\n",iThreadID);
			
#if PRINTDEBUGS == 1
			printf("User%d: Read Found\n", iThreadID);
#endif
			
			//Update the state of the User Thread to STATE4
			userFileToken[iThreadID].gStateOfUserThreads = STATE4;
			
#if PRINTDEBUGS == 1
			printf("User%d: Pause for a short duration to read tweets\n", iThreadID);
#else
			printf("\tUser%d Pausing for a short duration to read tweets\n", iThreadID);
#endif
			//Signal the Streamer Thread to continue with other users.
			sem_post(&sem_user_to_streamer[iThreadID]);

#if PRINTDEBUGS == 1
			printf("User%d: Signal Sent: From=User%d To=Streamer: Done Reading\n",iThreadID, iThreadID);
#endif

		}
		//Check if the line contains the 'Exit'
		else if(fnLineContains(line, "Exit") == 1)
		{

			printf("User%d: Exit\n",iThreadID);
			
#if PRINTDEBUGS == 1
			printf("\nUser%d: Exit Found: User%d Terminating\n", iThreadID, iThreadID);
#endif
		
			//Update the state of the User Thread to STATE5
			userFileToken[iThreadID].gStateOfUserThreads = STATE5;
			
			//Signal the Streamer Thread to continue with other users.
			sem_post(&sem_user_to_streamer[iThreadID]);

#if PRINTDEBUGS == 1
			printf("User%d: pthread_exit\n",iThreadID);
#else
			printf("\tUser%d is Exiting\n",iThreadID);
#endif
			//Exit the User(i) Thread
			pthread_exit(0);
			return NULL;
		}
	}
}

/**
 * Function called by Streamer threads to send/receive data.
 */
void *thread_streamer_fn(void *data)
{
	int i = 0;
	
	//Signal the first User Thread to Start Executing
	sem_post(&sem_streamer_to_user[i]);

#if PRINTDEBUGS == 1
	printf("Streamer: Signal Sent: From=Streamer To=User%d\n", i);
#endif
	
	while(1)
	{
		//Wait for Signal from the User Thread
		sem_wait(&sem_user_to_streamer[i]);

#if PRINTDEBUGS == 1
		printf("Streamer: Signal Received: From=User%d To=Streamer\n",i);
#endif
		//If the change in state of User(i) is STATE0
		if(userFileToken[i].gStateOfUserThreads == STATE0)
		{

#if PRINTDEBUGS == 1
			printf("Streamer: State Update for User%d, State: %d\n",i, userFileToken[i].gStateOfUserThreads);
			printf("Streamer: Tweeter No Work for you\n");
#endif

		}
		//If the change in state of User(i) is STATE1
		else if(userFileToken[i].gStateOfUserThreads == STATE1)
		{

#if PRINTDEBUGS == 1
			printf("Streamer: State Update for User%d, State: %d\n",i, userFileToken[i].gStateOfUserThreads);
			printf("Streamer: Tweeter No Work for you\n");
#endif

		}
		//If the change in state of User(i) is STATE2
		//When the user is tweeting
		else if(userFileToken[i].gStateOfUserThreads == STATE2)
		{

#if PRINTDEBUGS == 1
			printf("Streamer: State Update for User%d, State: %d\n",i, userFileToken[i].gStateOfUserThreads);
			printf("Streamer: Tweeter Work for you: Add %s", userFileToken[i].tweetTag);
#endif
			//Signal the Tweeter to Add the Tweet into the Repository
			sem_post(&sem_streamer_to_tweeter);

#if PRINTDEBUGS == 1
			printf("Streamer: Signal Sent: From=Streamer To=Tweeter: Add Tweet to Repository\n");
#endif
			//Wait for Signal from Tweeter that it has successfully added the Tweet into the Repository.
			sem_wait(&sem_tweeter_to_streamer);

#if PRINTDEBUGS == 1
			printf("Streamer: Signal Received: From=Tweeter To=Streamer: Tweet Added Successfully to Repository\n");
#endif
			//Signal the User that its tweet has been added successfully into the Repository by The Tweeter Thread
			sem_post(&sem_streamer_to_user[i]);
			
#if PRINTDEBUGS == 1
			printf("Streamer: Signal Sent: From=Streamer To=User%d\n",i);
#endif
			//Wait for Signal from the User Thread to conitnue with other users.
			sem_wait(&sem_user_to_streamer[i]);
		}
		//If the change in state of User(i) is STATE3
		//When user wants to follow
		else if(userFileToken[i].gStateOfUserThreads == STATE3)
		{

#if PRINTDEBUGS == 1
			printf("Streamer: State Update for User%d, State: %d\n",i, userFileToken[i].gStateOfUserThreads);
			printf("Streamer: Tweeter Work for you: Follow\n");
#endif
			//Signal to Tweeter to start follow a topic
			sem_post(&sem_streamer_to_tweeter);

#if PRINTDEBUGS == 1
			printf("Streamer: Signal Sent: From=Streamer To=Tweeter: Follow this Topic for User%d\n", i);
#endif
			
			while(1)
			{
				//Check if follow has been completed
				if(1 == iFollowCompleteFlag)
				{
					break;
				}
				
				//Wait for Signal from User to follow a tweet
				sem_wait(&sem_u_to_s_follow);

#if PRINTDEBUGS == 1
				printf("Streamer: Signal Recevied: From=User%d To=Streamer: Follow: %s\n", i, userFileToken[i].followTag);
#endif
				//Signal the Tweeter to follow a topic.
				sem_post(&sem_s_to_t_follow);

#if PRINTDEBUGS == 1
				printf("Streamer: Signal Sent: From=Streamer To=Tweeter: Follow: %s\n", userFileToken[i].followTag);
#endif
				//Wait for Signal from the Tweeter that it has added the follow tweet into the output buffer
				sem_wait(&sem_t_to_s_output);

#if PRINTDEBUGS == 1
				printf("Streamer:::::%s",userFileToken[i].outputBuffer);
#endif
				//Signal the User that the follow tweet has been added into the buffer and it can pick it up from there.
				sem_post(&sem_s_to_u_output);
				
#if PRINTDEBUGS == 1
				printf("Streamer: Signal Sent: From=Streamer To=User%d: Output\n", i);
#endif
			}
			//Wait for the signal from the Tweeter to indicate the end of control from tweeter for following a topic.
			sem_wait(&sem_tweeter_to_streamer);

#if PRINTDEBUGS == 1
			//printf("Streamer: Signal Recevied: From=Tweeter To=Streamer: outputBuffer updated Successfully\n");
#endif
			//Signal the User that all tweets have been followed.
			sem_post(&sem_streamer_to_user[i]);
			
#if PRINTDEBUGS == 1
			//printf("Streamer: Signal Sent: From=Streamer To=User%d: outputBuffer has the follow topic tweets\n",i);
#endif
			//Wait for Signal from the user to continue 
			sem_wait(&sem_user_to_streamer[i]);
			
		}
		//If the change in state of User(i) is STATE4
		else if(userFileToken[i].gStateOfUserThreads == STATE4)
		{

#if PRINTDEBUGS == 1
			printf("Streamer: State Update for User%d, State: %d\n",i, userFileToken[i].gStateOfUserThreads);
			printf("Streamer: Tweeter No Work for you\n");
#endif

		}
		//If the change in state of User(i) is STATE5
		else if(userFileToken[i].gStateOfUserThreads == STATE5)
		{
			
#if PRINTDEBUGS == 1
			printf("Streamer: State Update for User%d, State: %d\n",i, userFileToken[i].gStateOfUserThreads);
			printf("Streamer: Tweeter No Work for you\n");
#endif

			/*
			* Check if all user threads have set state to exit,
			* if yes, then exit the streamer thread.
			* */
			int j = 0;
			int count = 0;
			for(j=0;j<iNoOfThreads;j++)
			{
				if(userFileToken[j].gStateOfUserThreads == STATE5)
				{
					count++;
				}
			}
#if PRINTDEBUGS == 1
			//printf("\n********************Streamer count=%d    iNoOfThreads=%d*************************\n",count, iNoOfThreads);
#endif
			
			if(count == iNoOfThreads)
			{

#if PRINTDEBUGS == 1
				//printf("\n\n**************Streamer Should Break Now**********************\n\n");
				printf("Streamer: Exit: pthread_exit\n");
#else
				printf("Streamer Thread Exiting\n");
#endif
				sem_post(&sem_streamer_to_tweeter);
				//Set the flag to indicate that Streamer has finished and exited, so that even Tweeter can exit.
				iStreamerFinishedFlag = 1;
				pthread_exit(0);
				return NULL;
			}
		}
		while(1)
		{
			//Increment the tHread counter and check if that particular Thread has exited, if exited then schedule the next thread which has not yet exited.
			i++;
			i = i % iNoOfThreads;
			if(userFileToken[i].gStateOfUserThreads == STATE5)
			{
				continue;
			}
			iCurrentThreadInProgress = i;
			userFileToken[i].gStateOfUserThreads = STATE0;
			//Signal the User to Continue, this the point where schduling happens
			sem_post(&sem_streamer_to_user[i]);
			break;
		}
		
#if PRINTDEBUGS == 1
		printf("********************************************************************************\n");
		printf("Streamer: Signal Sent: From=Streamer To=User%d: Start\n", i);
#endif
	}
}

/**
 * Function called by tweeter threads to send/receive data.
 */
void *thread_tweeter_fn(void *data)
{
	while(1)
	{
		//Wait for Signal from the Streamer Thread to Add or Follow a Tweet.
		sem_wait(&sem_streamer_to_tweeter);

#if PRINTDEBUGS == 1
		//printf("Tweeter: Signal Received: From=Streamer To=Tweeter: State Update:%d for User%d\n", userFileToken[iCurrentThreadInProgress].gStateOfUserThreads, iCurrentThreadInProgress);
#endif

		//If request is to add to repository
		if(userFileToken[iCurrentThreadInProgress].gStateOfUserThreads == STATE2)
		{
			//This function call adds the tweet into the Repository.
			insert_into_Repository();
#if PRINTDEBUGS == 1
			//printf("Tweeter: Add Tweet: From=outputBuffer of User%d To=Repository\n",iCurrentThreadInProgress);
			printf("Tweeter: Tweet added successfully to repository\n");
#endif
			//Signal the Streamer to indicate that the tweet has been added into the repository.
			sem_post(&sem_tweeter_to_streamer);

#if PRINTDEBUGS == 1
			//printf("Tweeter: Signal Sent: From=Tweeter To=Streamer: Tweet Added Successfully\n");
#endif

		}
		else if(userFileToken[iCurrentThreadInProgress].gStateOfUserThreads == STATE3)
		{
			//Wait for Signal from the Streamer Thread to start following a tweet topic
			sem_wait(&sem_s_to_t_follow);

#if PRINTDEBUGS == 1
			printf("Tweeter: Signal Received: From=Streamer To=Tweeter: Follow: %s\n",userFileToken[iCurrentThreadInProgress].followTag);
#endif

			struct TweetNode* local_tweet_temp_node = NULL;
			//This function call checks if any related tweet is present in repository.
			local_tweet_temp_node = isFollowTagInRepository();
			if(local_tweet_temp_node == NULL)
			{
				//If no related tweets are present then follow complete flag can be set.
				iFollowCompleteFlag = 1;
				//Signal the Streamer that output message is available in buffer.
				sem_post(&sem_t_to_s_output);
			}
			else
			{
				while(1)
				{
#if PRINTDEBUGS == 1
					//printf("%s:::%s\n",local_tweet_temp_node->handleName, local_tweet_temp_node->buffer);
#endif
					//Copy the message buffer from the repository into the output buffer.
					strcpy(userFileToken[iCurrentThreadInProgress].outputBuffer, local_tweet_temp_node->buffer);
					//Copy the handle Name from the repository into the output Handle buffer.
					strcpy(userFileToken[iCurrentThreadInProgress].oHandleName, local_tweet_temp_node->handleName);
					//Signal the Streamer that message has been put into output buffer.
					sem_post(&sem_t_to_s_output);
					//Wait for Signal from the Streamer to follow the next tweet from the repository.
					sem_wait(&sem_s_to_t_follow);
					local_tweet_temp_node = (struct TweetNode *)local_tweet_temp_node->next;
					if(local_tweet_temp_node == NULL)
					{
						iFollowCompleteFlag = 1;
						char *noMoreMessage = "No More Related Tweets";
						strcpy(userFileToken[iCurrentThreadInProgress].outputBuffer, noMoreMessage);
						sem_post(&sem_t_to_s_output);
						break;
					}
				}
			}
			//Signal the Stremer that follow has been completed and it can continue with next user.
			sem_post(&sem_tweeter_to_streamer);

#if PRINTDEBUGS == 1
			//printf("Tweeter: Signal Sent: From=Tweeter To=Streamer: OutputBuffer Updated with required tweets for follow topic\n");
#endif

		}
		else
		{
			/*
			* Check if all user threads have set state to exit,
			* if yes, then exit the streamer thread.
			* */
			int j = 0;
			int count = 0;
			for(j=0;j<iNoOfThreads;j++)
			{
				if(userFileToken[j].gStateOfUserThreads == STATE5)
				{
					count++;
				}
			}
#if PRINTDEBUGS == 1
			//printf("\n********************Tweeter count=%d    iNoOfThreads=%d*************************\n",count, iNoOfThreads);
#endif
			
			if(count == iNoOfThreads)
			{

#if PRINTDEBUGS == 1
				//printf("\n\n**************Tweeter Should Break Now**********************\n\n");
				printf("Tweeter: Exit: pthread_exit\n");
#else
				printf("Tweeter Thread Exiting\n");
#endif
				pthread_exit(0);
				return NULL;
			}
		}
	}
}

/**
 * Function called by user thread to determine the thread state
 * It compares the input string with the string in the find variable
 * and return 1 if the string contains the find strinf
 * else it returns 0.
 */
int fnLineContains(char *input, const char *find)
{
    do
    {
        const char *p, *q;
        for (p = input, q = find; *q != '\0' && *p == *q; p++, q++) {}
        if (*q == '\0')
        {
            return 1;
        }
    } while (*(input++) != '\0');
    return 0;
}

/**
 * Function called by Tweeter thread to add the tweets into the repository.
 */
void insert_into_Repository()
{
	struct TweeterRepository *new_tag_node = NULL;
	struct TweetNode *new_tweet_node = (struct TweetNode *)malloc(sizeof(struct TweetNode));

	//Populate the New Tweet Node
	strcpy(new_tweet_node->buffer, userFileToken[iCurrentThreadInProgress].inputBuffer);
	strcpy(new_tweet_node->handleName, userFileToken[iCurrentThreadInProgress].handleName);
	new_tweet_node->next = NULL;

	//Check if the repository is empty
	if(tr_head_node == NULL)
	{
		new_tag_node = (struct TweeterRepository *)malloc(sizeof(struct TweeterRepository));
		
		strcpy(new_tag_node->tweetTag, userFileToken[iCurrentThreadInProgress].tweetTag);
		new_tag_node->nextTag = NULL;
		
		new_tag_node->nextTweet = (struct TweetNode*)new_tweet_node;
		tr_head_node = new_tag_node;
	}
	else
	{
		int iNewTweetNode = 0;
		tr_temp_node = tr_head_node;
		while(1)
		{
			//printf("tr_temp_node->tweetTag =%s\tuserFileToken[iCurrentThreadInProgress].tweetTag=%s\n",tr_temp_node->tweetTag, userFileToken[iCurrentThreadInProgress].tweetTag);
			//Check if any of the existing Tweet Tag names matches with the Tweet Tag to be inserted.
			if(0 == strcmp(tr_temp_node->tweetTag, userFileToken[iCurrentThreadInProgress].tweetTag))
			{
				tweet_temp_node = (struct TweetNode *)tr_temp_node->nextTweet;
				while(tweet_temp_node->next != NULL)
				{
					tweet_temp_node = (struct TweetNode *)tweet_temp_node->next;
				}
				tweet_temp_node->next = (struct TweetNode*)new_tweet_node;
				iNewTweetNode = 1;
				break;
			}
			if(tr_temp_node->nextTag != NULL)
			{
				tr_temp_node = (struct TweeterRepository*)tr_temp_node->nextTag;
			}
			else
			{
				break;
			}
		}
		//Check if any of the existing Tweet Tag names doesn't match with the Tweet Tag to be inserted then new TweetRepository Node needs to be created.
		if(iNewTweetNode == 0)
		{
			new_tag_node = (struct TweeterRepository*)malloc(sizeof(struct TweeterRepository));
			
			strcpy(new_tag_node->tweetTag, userFileToken[iCurrentThreadInProgress].tweetTag);
			new_tag_node->nextTag = NULL;
			
			new_tag_node->nextTweet = (struct TweetNode*)new_tweet_node;
			
			tr_temp_node->nextTag = (struct TweeterRepository*)new_tag_node;
		}
	}
	print_Repository();
}

/**
 * This function can be called at any time to print the entire contents of
 * the repository.
 */
void print_Repository()
{

#if PRINTDEBUGS == 1
	printf("***************************************Start of Repository Contents***************************************\n");
#endif

	tr_temp_node = tr_head_node;
	while(tr_temp_node != NULL)
	{

#if PRINTDEBUGS == 1
		printf("%s",tr_temp_node->tweetTag);
#endif

		tweet_temp_node = tr_temp_node->nextTweet;
		while(tweet_temp_node != NULL)
		{

#if PRINTDEBUGS == 1
			printf("\t%s:::%s\n",tweet_temp_node->handleName, tweet_temp_node->buffer);
#endif

			tweet_temp_node = tweet_temp_node->next;
		}
		tr_temp_node = (struct TweeterRepository*)tr_temp_node->nextTag;
	}

#if PRINTDEBUGS == 1
	printf("***************************************End of Repository Contents***************************************\n");
#endif

}

/**
 * This function is called by the Tweeter Thread and is used to determine
 * if the follow topic is present in the repository or not.
 * It return null if the follow topic is not present in repository
 * and it return the tweet node pointer if it present in the repository.
 */
struct TweetNode* isFollowTagInRepository()
{
	print_Repository();
	//If the Head Node is null, meaning the repository is still empty.
	if(tr_head_node == NULL)
	{
		char *emptyMessage = "Repository is Empty";
		strcpy(userFileToken[iCurrentThreadInProgress].outputBuffer, emptyMessage);
		return NULL;
	}
	else
	{
		tr_temp_node = tr_head_node;
		//Loop untill the end of the repository is reached.
		while(tr_temp_node->nextTag != NULL)
		{
			//Check of the follow topic tag matches with the repository.
			if(0 == strcmp(tr_temp_node->tweetTag, userFileToken[iCurrentThreadInProgress].followTag))
			{
				tweet_temp_node = (struct TweetNode *)tr_temp_node->nextTweet;
				return tweet_temp_node;
			}
			tr_temp_node = (struct TweeterRepository*)tr_temp_node->nextTag;
		}
		
		char *noTweetMessage = "No Related Tweets in Repository";
		strcpy(userFileToken[iCurrentThreadInProgress].outputBuffer, noTweetMessage);
		return NULL;
	}
}
