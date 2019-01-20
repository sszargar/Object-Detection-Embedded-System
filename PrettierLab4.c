#include "gpiolib_addr.h" //include all relevant libraries
#include "gpiolib_reg.h"
#include <stdint.h>
#include <stdio.h>		//for the printf() function
#include <fcntl.h>
#include <linux/watchdog.h> 	//needed for the watchdog specific constants
#include <unistd.h> 		//needed for sleep
#include <sys/ioctl.h> 		//needed for the ioctl function
#include <stdlib.h> 		//for atoi
#include <time.h> 		//for time_t and the time() function
#include <sys/time.h>           //for gettimeofday()
#include <signal.h>
#include <errno.h>

#define LASER1_PIN_NUM 4 //this will replace LASER1_PIN_NUM with 4 when compiled
#define LASER2_PIN_NUM 17 //this will replace LASER2_PIN_NUM with 17 when compiled

//Below is a macro that had been defined to output appropriate logging messages
//You can think of it as being similar to a function
//logFile or statsFile  - will be the file pointer to the log file
//time        - will be the current time at which the message is being printed
//programName - will be the name of the program, in this case it will be Lab4Sample
//str         - will be a string that contains the message that will be printed to the file.
#define PRINT_LOG(logFile, time, programName, str) \
	do{ \
			fprintf(logFile, "%s : %s : %s", time, programName, str); \
			fflush(logFile); \
	}while(0)

//stat 			- stat about objects entering, laser 1 breaks, etc.
#define PRINT_STATS(statsFile, time, programName, str, stat) \
	do{ \
			fprintf(statsFile, "%s : %s : %s: %d", time, programName, str, stat); \
			fflush(statsFile); \
	}while(0)

#ifndef MARMOSET_TESTING
//this function accepts an errorCode
//errorCode 1 means that the config file could not be opened
//errorCode 2 means that the GPIO could not be initialized
void errorMessage(const int errorCode) {
   fprintf(stderr, "An error occured; the error code was %d \n", errorCode);
}

//this function initializes the GPIO pins
GPIO_Handle initializeGPIO() {
   GPIO_Handle gpio;
   gpio = gpiolib_init_gpio();
   if (gpio == NULL) {
      errorMessage(2);
   }
   return gpio;
}

//this function gets the current state of the laser by taking the diode number
//as a parameter instead of a pin number. it accepts the diode number (1 or 2) and outputs
//a '0' if the laser beam is not reaching the diode, a 1 if the laser
//beam is reaching the diode or -1 if an error occurs.
int laserDiodeStatus(GPIO_Handle gpio, int diodeNumber) {
   if (gpio == NULL) {
      return -1;
   }

   if (diodeNumber == 1) {
      uint32_t level_reg1 = gpiolib_read_reg(gpio, GPLEV(0));

      if (level_reg1 & (1 << LASER1_PIN_NUM)) {
         return 1;
      } else {
         return 0;
      }
   }

   if (diodeNumber == 2) {
      uint32_t level_reg2 = gpiolib_read_reg(gpio, GPLEV(0));

      if (level_reg2 & (1 << LASER2_PIN_NUM)) {
         return 1;
      } else {
         return 0;
      }
   } else {
      return -1;
   }
}

#endif

//This is a function used to read from the config file.
void readConfig(FILE * configFile, int * timeout, char * logFileName, char * statsFileName) {
   //Loop counter
   int i = 0;
   int j = 0;
   int k = 0;

   //A char array to act as a buffer for the file
   char buffer[255];

   //The value of the timeout variable is set to zero at the start
   *timeout = 0;

	//This is a variable used to track which input we are currently looking
	//for (timeout, logFileName or numBlinks)
	int input = 0;

	//This will 
	//fgets(buffer, 255, configFile);
	//This will check that the file can still be read from and if it can,
	//then the loop will check to see if the line may have any useful 
	//information.
	while(fgets(buffer, 255, configFile) != NULL)
	{
		i = 0;
		//If the starting character of the string is a '#', 
		//then we can ignore that line
		if(buffer[i] != '#')
		{
			while(buffer[i] != 0)
			{				
				//This if will check the value of timeout
				if(buffer[i] == '=' && input == 0)
				{
					//The loop runs while the character is not null
					while(buffer[i] != 0)
					{
						//If the character is a number from 0 to 9
						if(buffer[i] >= '0' && buffer[i] <= '9')
						{
							//Move the previous digits up one position and add the
							//new digit
							*timeout = (*timeout *10) + (buffer[i] - '0');
						}
						i++;
					}
					input++;
				}
				else if(buffer[i] == '=' && input == 1) //This will find the name of the log file
				{
					int j = 0;
					//Loop runs while the character is not a newline or null
					while(buffer[i] != 0  && buffer[i] != '\n')
					{
						//If the characters after the equal sign are not spaces or
						//equal signs, then it will add that character to the string
						if(buffer[i] != ' ' && buffer[i] != '=')
						{
							logFileName[j] = buffer[i];
							j++;
						}
						i++;
					}
					//Add a null terminator at the end
					logFileName[j] = 0;
					input++;
				}
				else if(buffer[i] == '=' && input == 2) //This will find the name of the stats file
				{
					int k = 0;
					//Loop runs while the character is not a newline or null
					while(buffer[i] != 0  && buffer[i] != '\n')
					{
						//If the characters after the equal sign are not spaces or
						//equal signs, then it will add that character to the string
						if(buffer[i] != ' ' && buffer[i] != '=')
						{
							statsFileName[k] = buffer[i];
							k++;
						}
						i++;
					}
					//Add a null terminator at the end
					statsFileName[k] = 0;
					input++;
				}
				else
				{
					i++;
				}
			}
		}
	}
}

//This function will get the current time using the gettimeofday function
void getTime(char * buffer) {
   //Create a timeval struct named tv
   struct timeval tv;

   //Create a time_t variable named curtime
   time_t curtime;

   //Get the current time and store it in the tv struct
   gettimeofday( & tv, NULL);

   //Set curtime to be equal to the number of seconds in tv
   curtime = tv.tv_sec;

   //This will set buffer to be equal to a string that in
   //equivalent to the current date, in a month, day, year and
   //the current time in 24 hour notation.
   strftime(buffer, 30, "%m-%d-%Y  %T.", localtime( & curtime));

}

//this function will count the number of objects that have entered or exited the room, and
//the number of times each laser was broken
int objectCount(char programName[]) {
   //Create a file pointer named configFile
   FILE * configFile;
   //Set configFile to point to the Lab4Sample.cfg file. It is
   //set to read the file.
   configFile = fopen("/home/pi/Lab4Sample.cfg", "r");

   //Output a warning message if the file cannot be openned
   if (!configFile) {
      errorMessage(1);
      return -1;
   }

   //Declare the variables that will be passed to the readConfig function
   int timeout;
   char logFileName[50];
   char statsFileName[50];

   //Call the readConfig function to read from the config file
   readConfig(configFile, &timeout, logFileName, statsFileName);

   //Close the configFile now that we have finished reading from it
   fclose(configFile);

   //Create a new file pointer to point to the log file
   FILE * logFile;

   //Set it to point to the file from the config file and make it append to the file when it writes to it.
   logFile = fopen(logFileName, "a");

   //create a new file pointer to point to the stats file
   FILE * statsFile;
   //set it to point to the file from the config file and make it append to the file when it writes to it
   statsFile = fopen(statsFileName, "a");

   //Check that the file opens properly.
   if (!configFile) {
      errorMessage(1);
      return -1;
   }

   //Create a char array that will be used to hold the time values
   char time[30];

    GPIO_Handle gpio = initializeGPIO(); //the gpio has to be initialized at the beginning of the function
   //Get the current time
   getTime(time);
   //Log that the GPIO pins have been initialized
   PRINT_LOG(logFile, time, programName, "The GPIO pins have been initialized\n\n");

   //This variable will be used to access the /dev/watchdog file
	int watchdog;

	//We use the open function here to open the /dev/watchdog file. If it does
	//not open, then we output an error message. We do not use fopen() because we
	//do not want to create a file if it doesn't exist
	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) {
		printf("Error: Couldn't open watchdog device! %d\n", watchdog);
		return -1;
	} 

   getTime(time);
   //Log that the watchdog file has been opened
   PRINT_LOG(logFile, time, programName, "The Watchdog file has been opened\n\n");

   //This line uses the ioctl function to set the time limit of the watchdog timer to 15 seconds
   ioctl(watchdog, WDIOC_SETTIMEOUT, &timeout);

   getTime(time);
   //Log that the Watchdog time limit has been set
   PRINT_LOG(logFile, time, programName, "The Watchdog time limit has been set\n\n");

   //The value of timeout will be changed to whatever the current time limit of the
   //watchdog timer is
   ioctl(watchdog, WDIOC_GETTIMEOUT, &timeout);

   //This print statement will confirm to us if the time limit has been properly
   //changed. The \n will create a newline character similar to what endl does.
   printf("The watchdog timeout is %d seconds.\n\n", timeout);

   enum State { START, BOTH_ON, L1_OFF, BOTH_OFF, L2_OFF, DONE };
   enum State s = START;
 
   int numberIn = 0; //the number of balls that moved into the room
   int numberOut = 0; //the number of balls that moved out of the room
   int direction = 0; //variable to see if the ball is going in or out of the room
  					        //when direction = -4, the ball is outside of the room; when direction = 4, the ball is inside the room
   int laser1Count = 0; //number of times L1 is broken
   int laser2Count = 0; //number of times L2 is broken

   int L1_status = 0; //initialize the status of the lasers at the beginning of the function
   int L2_status = 0;

   //the state machine will loop through continuously
   while (1) {
   //This ioctl call will write to the watchdog file every 15 seconds and prevent the system from rebooting.
	ioctl(watchdog, WDIOC_KEEPALIVE, 0);
	getTime(time);
	//Log that the Watchdog was kicked
	PRINT_LOG(logFile, time, programName, "The Watchdog was kicked\n\n");

      L1_status = laserDiodeStatus(gpio, 1);
      //if L1_status = 0, the laser does not reach the photodiode 
      //if L1_status = 1, the laser reaches the photodiode 
      L2_status = laserDiodeStatus(gpio, 2); 
      //if L2_status = 0, the laser does not reach the photodiode
      //if L2_status = 1, the laser reaches the photodiode 

      usleep(10000); //allow the program to pause slightly before beginning the state machine

      switch (s) {
      case START:
         numberIn = 0; //initialize the number of objects that have moved in
         numberOut = 0; //initialize the number of objects that have moved out

         if (L1_status == 1 && L2_status == 1) //both lasers reach the photodiodes, so no objects are in the doorway)
         {
            s = BOTH_ON;
         }

         break;

      //both lasers are turned on
      case BOTH_ON:

         printf("Direction (should be 4 or -4): %d \n ", direction);

         if (L1_status == 0 && L2_status == 1) {
            s = L1_OFF; //the L1 laser does not reach the photodiode, but the the L2 laser reaches the photodiode so an object is halfway in 
            direction = 1;
            laser1Count++; //the L1 laser has been broken

            getTime(time); //get the current time to be printed
            PRINT_STATS(statsFile, time, programName, "Number of times the 1st laser was broken: \n", laser1Count);
            //print a statistic about the laser break count to the stats file
         }

         if (L1_status == 1 && L2_status == 0) {
            s = L2_OFF; //the L1 laser reaches the photodiode, but the the L2 laser does not reach the photodiode so an object is halfway out 
            direction = -1;
            laser2Count++; //the L2 laser has been broken

            getTime(time);
            PRINT_STATS(statsFile, time, programName, "Number of times the 2nd laser was broken: \n", laser2Count);
         }
 
         break;

      //the L1 laser is turned off 
      case L1_OFF:

         printf("Direction (should be 1 or -3): %d \n ", direction);

         if (L1_status == 0 && L2_status == 0) {
            s = BOTH_OFF; //both lasers do not reach the photodiode, so the object is in the doorway 
            direction++;
            laser2Count++; //laser 2 is broken; laser 1 is also broken but the count isn't incremented because it was already broken

            getTime(time);
            PRINT_STATS(statsFile, time, programName, "Number of times the 2nd laser was broken: \n", laser2Count);
         }

         if (L1_status == 1 && L2_status == 1) {
            direction--;

            if (direction == -4) {
               printf("Number out %d \n ", numberOut);
               numberOut++; //the ball has gone out of the room

               getTime(time);
               PRINT_STATS(statsFile, time, programName, "Number of objects that left the room: \n", numberOut);
               //print a statistic about the number of objects in the room to the stats file
            }

            s = BOTH_ON; //both lasers reach the photodiode, so no objects are in the doorway
         }

         break;

      //both lasers are turned off
      case BOTH_OFF:

         printf("Direction (should be 2 or -2): %d \n ", direction);

         if (L1_status == 1 && L2_status == 0) {
            s = L2_OFF; //the L1 laser reaches the photodiode, but the the L2 laser does not reach the photodiode so an object is halfway out
            direction++;
            //laser counts aren't incremented because they were both already turned off
         }

         if (L1_status == 0 && L2_status == 1) {
            s = L1_OFF; //the L1 laser does not reach the photodiode, but the the L2 laser reaches the photodiode so an object is halfway in
            direction--;
            //laser counts aren't incremented because they were both already turned off
         }

         break;

      //the L2 laser is turned off
      case L2_OFF:

         printf("Direction (should be 3 or -1): %d \n ", direction);

         if (L1_status == 0 && L2_status == 0) {
            s = BOTH_OFF; //both lasers do not reach the photodiode, so the object is in the doorway
            direction--;
            laser1Count++; //laser 1 is broken; laser 2 is also broken but the count isn't incremented because it was already broken

            getTime(time);
            PRINT_STATS(statsFile, time, programName, "Number of times the 1st laser was broken: \n", laser1Count);
         }

         if (L1_status == 1 && L2_status == 1) {
            direction++;

            if (direction == 4) {
               printf("Number in %d \n ", numberIn);
               numberIn++; //the ball has gone into the room

               getTime(time);
               PRINT_STATS(statsFile, time, programName, "Number of objects that entered the room: \n", numberOut);
            }

            s = BOTH_ON; //both lasers reach the photodiode, so no objects are in the doorway
         }

         break;

      case DONE:
         //Free the GPIO now that the program is over.
         gpiolib_free_gpio(gpio);

         getTime(time);
         //Log that the GPIO pins were freed
         PRINT_LOG(logFile, time, programName, "The GPIO pins have been freed\n\n");
         break;
      }
   }

}


#ifndef MARMOSET_TESTING

int main(const int argc, const char * const argv[])
{
   //Create a string that contains the program name
   const char * argName = argv[0];

   //These variables will be used to count how long the name of the program is
   int i = 0;
   int namelength = 0;

   while (argName[i] != 0) {
      namelength++;
      i++;
   }

   char programName[namelength]; //declare a string that will later become the program name, Lab4

   i = 0;

   //Copy the name of the program without the ./ at the start of argv[0]
   while (argName[i + 2] != 0) {
      programName[i] = argName[i + 2];
      i++;
   } 

   programName[namelength-1] = 0; // insert null terminator 0 at end of char array

   objectCount(programName); //call objectCount using programName

   return 0;
}

#endif