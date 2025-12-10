#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "imu_driver.h"


#define I2C_BUS_FILE "/dev/i2c-2"

static int difficulty_lvl = 1;


int init_imu() {
	int imu_status;
	int imu_file_handle;
	if ((imu_file_handle = open(I2C_BUS_FILE, O_RDWR)) < 0) {
		perror("Failed to open i2c bus\n");
		return -1;
	}

	imu_status = imu_init(imu_file_handle);
	if (imu_status != 0) {
		perror("Could not initialize IMU device\n");
		return -1;
	}

	return imu_file_handle;
}


int calc_travel_pos(imu_data_t imu_data, int curr_pos) {
	int delta_x;
	int scaling = 5;
	delta_x = (int)(-1 * imu_data.gyro_x / scaling) * difficulty_lvl;
	//delta_x = (int)(imu_data.gyro_y / scaling) * difficulty_lvl;
	//delta_x = (int)(imu_data.gyro_z / scaling) * difficulty_lvl;
	
	curr_pos = curr_pos + delta_x;
	if (curr_pos > 450) {
		curr_pos = 450;
	}
	else if (curr_pos < 0) {
		curr_pos = 0;
	}

	return curr_pos;
}

int rand_spawn_meteor() {
	int max = 420;
	int min = 0;
	int odds = 200;
	int rand_chance;
	int rand_pos;

	odds = odds / (4 * difficulty_lvl);
	rand_chance = (rand() % (odds)) + 1;
	
	if (rand_chance == 1) {
		rand_pos = (rand() % (max - min + 1)) + min;
	}
	else {
		rand_pos = -1;
	}

	return rand_pos;
}



int main(int argc, char **argv) {
	//check to see if difficulty was set
	if (argc != 2) {
		printf("No difficulty selected!\nChoose between 1 - 10\n");
		return 1;
	}

	//seed random choices
	srand(time(NULL));

	//set difficulty level
	difficulty_lvl = atoi(argv[1]);
	
	int pFile;
	FILE* highscore_file;
	//pen device file
	pFile = open("/dev/meteor_dash", O_WRONLY);

	//error check for device opening
	if (pFile < 0) {
        	printf("Error opening file!\n");
        	return 1;
    }

	//initialize character position
	int character_pos = 100;

	//initialize imu
	int imu_file_handle;
	imu_file_handle = init_imu();
	
	//error check for imu reading
	if (imu_file_handle == -1) {
		close(pFile);
		return 1;

	}

	//init variables for loop
	imu_data_t imu_reading;
	int meteor_pos;

	char buffer[32];
	char score_buf[256];

	//calc obs falling rate
	int block_fallrate = 4 + (difficulty_lvl / 2);
	
	//send block falling rate into buffer to write
	int bytes_written = sprintf(buffer, "-1,%d,", block_fallrate);

	//write block falling rate to device file
	size_t written_elements = write(pFile, buffer, bytes_written);
	//error check
	if (written_elements == -1) {
		printf("Error writing difficulty fall rate\n");
		close(pFile);
		return 1;
	}

	int score = 0;
	int play = 1;
	while (play == 1) {
	int GAMEOVER = 0;
	char play_again;
	//game loop
	while (GAMEOVER == 0) {

		//delay for --- msec maybe necessary?
		usleep(50 * 1000);
		score += difficulty_lvl;
		if (((score % 400) == 0) && (difficulty_lvl < 10)){
			difficulty_lvl += 1;
			printf("Moving up in difficulty... current score: %d\n", score);
			
			int block_fallrate = 4 + (difficulty_lvl / 2);
	
			//send block falling rate into buffer to write
			int bytes_written = sprintf(buffer, "-1,%d,", block_fallrate);

			//write block falling rate to device file
			size_t written_elements = write(pFile, buffer, bytes_written);
			//error check
			if (written_elements == -1) {
				printf("Error writing difficulty fall rate\n");
				close(pFile);
				return 1;
			}
		
		}

		//read imu data
		imu_reading = imu_read(imu_file_handle);
		
		//calculate change in position
		character_pos = calc_travel_pos(imu_reading, character_pos);
		

		//randomly spawn a meteor at a random location
		meteor_pos = rand_spawn_meteor();

		//format data to write to device file
		int bytes_written = sprintf(buffer, "%d,%d,", character_pos, meteor_pos);

		//write latest data to device file
		size_t written_elements = write(pFile, buffer, bytes_written);
		int err_num = errno;
		//error check
		//check for termination signal
		if (written_elements == -1) {
			if (err_num == 2) {
				err_num = 0;
				printf("GAME OVER! YOU HIT A METEOR!\n");
				printf("Your score was: %d\n", score);
				close(pFile);
				
				highscore_file = fopen("leaderboard.txt", "r+");
				if (highscore_file == NULL) {
					printf("error accessing leaderboard. SORRY!\n");
					return 1;
				}
				
				fgets(score_buf, sizeof(score_buf), highscore_file);
				fclose(highscore_file);
				if (atoi(score_buf) < score) {
					printf("New Highscore! Congrats!\n");
					fopen("leaderboard.txt", "w");
					fprintf(highscore_file, "%d\n", score);
					fclose(highscore_file);
				}
				
				GAMEOVER = 1;
			}
			else {
				printf("Error writing elements\n");
				close(pFile);
				return 1;
			}
		}

	}
	printf("Play again? (y/n) \n");
	scanf(" %c", &play_again);
	
	if (play_again == 'y') {
		play = 1;
		GAMEOVER = 0;
		score = 0;
		difficulty_lvl = 1;
		pFile = open("/dev/meteor_dash", O_WRONLY);

		//error check for device opening
		if (pFile < 0) {
        		printf("Error opening file!\n");
        		return 1;
    		}

	}
	else if (play_again == 'n') {
		play = 0;
		return 0;
	}
	else {
		printf("Incorrect input... GAMEOVER/n");
		play = 0;
		return 0;
	}
	}
	
}

