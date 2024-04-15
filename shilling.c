/*
Created by Ryan Srichai
https://github.com/Known4225
MIT licensed, this is open source software.

13.04.24:
This project creates a movie recommender based on the MovieLens latest small database (as of 13.04.24)
It has three implemented prediction algorithms:
 - User-user collaborative recommendation (from https://www.geeksforgeeks.org/user-based-collaborative-filtering)
 - Item-item collaborative recommendation (not implemented, i was lazy)
 - SVD (not implemented, once again, laziness)
It has three metrics as well:
 - MAE
 - RMSE
 - NDCG
And two special made up metric:
 - ARL (Average rating * log) - a popularity/goodness metric applied to each item
 - SHE (Shill effectiveness) - counts the number of times the shilled item appears in test users recommendation lists, weighted so closer to the top is better

Notes/Results:
with srand = time(NULL), 100 shills, shill fill and blend amounts are 10

Shill Strategy               Algorithm      SHE (no shills)     SHE (with 100 shills)    ShillPush    ShillNuke       Effect
SHILL_NAIVE_PUSH             user-user           122                    122                  1           NA           None
SHILL_RANDOM_FILL_PUSH       user-user           78                     105                  1           NA           Correct
SHILL_TARGETED_FILL_PUSH     user-user           111                    139                  1           NA           Correct
SHILL_RANDOM_BLEND_PUSH      user-user           532                    658                  1           NA           Correct
SHILL_TARGETED_BLEND_PUSH    user-user           89                     83                   1           NA           Opposite

SHILL_NAIVE_NUKE             user-user          1719                   1719                  NA          278          None
SHILL_RANDOM_FILL_NUKE       user-user          1601                   1951                  NA          278          Opposite
SHILL_TARGETED_FILL_NUKE     user-user          1866                   1652                  NA          278          Correct
SHILL_RANDOM_BLEND_NUKE      user-user          1781                   1717                  NA          278          Correct
SHILL_TARGETED_BLEND_NUKE    user-user          1723                   1625                  NA          278          Correct

I'm going to make one more table with srand set to a fixed value
with srand = 1713216227, 100 shills, shill fill and blend amounts are 10

Shill Strategy               Algorithm      SHE (no shills)     SHE (with 100 shills)    ShillPush    ShillNuke       Effect
SHILL_NAIVE_PUSH             user-user           394                    394                  1           NA           None
SHILL_RANDOM_FILL_PUSH       user-user           394                    515                  1           NA           Correct
SHILL_TARGETED_FILL_PUSH     user-user           394                    528                  1           NA           Correct
SHILL_RANDOM_BLEND_PUSH      user-user           394                    515                  1           NA           Correct
SHILL_TARGETED_BLEND_PUSH    user-user           394                    293                  1           NA           Opposite

SHILL_NAIVE_NUKE             user-user          1822                   1822                  NA          278          None
SHILL_RANDOM_FILL_NUKE       user-user          1822                   2197                  NA          278          Opposite
SHILL_TARGETED_FILL_NUKE     user-user          1822                   1641                  NA          278          Correct
SHILL_RANDOM_BLEND_NUKE      user-user          1822                   1719                  NA          278          Correct
SHILL_TARGETED_BLEND_NUKE    user-user          1822                   1766                  NA          278          Correct

I'm going to verify the values are the same on both windows and linux
But if they are, we have some good results

Analysing:
So it seems like certain strategies are more/less/counter effective.
looks like the SHILL_TARGETED_BLEND_PUSH literally doesn't work. The item that is "pushed" is actually anti-pushed and is recommended less by the user-user system
same goes with the SHILL_RANDOM_FILL_NUKE, the nuked item goes on more recommendation lists.
So honestly both of these strategies still "work" but you just have to apply them oppositely.
It doesn't make sense to me how rating shawshank redemption a 0/5 100 times would make it appear on MORE people's recommendation lists,
but apparently this is the world we live in

The shill strategies that categorically didn't work are the naive shills, the ones that just rate the one item and that's it.
I'd guess that's because they were a 0% similarity with all of the "real" users and thus had 0 impact on their recommendations
It literally didn't have any effect on the RMSE or MAE either, meaning the algorithm straight up ignored them.
*/
#include "include/csvParser.h"
#include <math.h>
#include <time.h>

/* shill strategy macros */
#define SHILL_NAIVE_PUSH             1
#define SHILL_RANDOM_FILL_PUSH       2
#define SHILL_TARGETED_FILL_PUSH     3
#define SHILL_RANDOM_BLEND_PUSH      4
#define SHILL_TARGETED_BLEND_PUSH    5

#define SHILL_NAIVE_NUKE             11
#define SHILL_RANDOM_FILL_NUKE       12
#define SHILL_TARGETED_FILL_NUKE     13
#define SHILL_RANDOM_BLEND_NUKE      14
#define SHILL_TARGETED_BLEND_NUKE    15

/* prediction method macros */
#define PREDICTION_USER_USER         1
#define PREDICTION_ITEM_ITEM         2
#define PREDICTION_SVD               3

/*
Movies are given an internal ID that does not skip numbers, these are not the same as the movieID from the dataset
Users also have an ID that does not skip, but it is currently equal to the userID from the ratings list
If a user, such as user 0, has no movies rated then we just have them logged with no movies rated
*/

typedef struct {
    list_t *movies; // loaded from movies.tsv
    list_t *trainRatings; // initially loaded from ratings.csv, but testSize are removed and added to testRatings
    int testSize; // this integer determines how large the testing set is
    list_t *testRatings; // size determined by testSize
    list_t *internalMovieID;
    list_t *trainUsers; // spec: [userID (int), movies rated (list of ints (internalID)), ratings (list of doubles)] , (each element is a list with these qualities)
    list_t *testUsers; // same spec as users. Shills are never test users (could change eventually, but in a way this is good because we don't care about accurate recommendations for the shills)
    list_t *shills; // separate list of shills with the same spec as users

    list_t *userReference; // cached locations of where users have rated items
    list_t *predictions; // spec: [userID, [movie0, movie1, movie2, movie3, movie4, ...]] (each element is a list with this format, movieIDInternals never skip)
    list_t *ranked; // top ranked movies based on users list, spec: [movieIDInternal (int), movieName (string), averageRank (double), numberOfRatings (int)] (each element is a list with these qualities)


    int shillPush; // the ID of the movie that the shills are pushing
    int shillPushFillAmount; // the number of movies that a push shill will rate 0/5
    int shillPushBlendAmount; // the number of movies that a blend shill will rate 3/5

    int shillNuke; // the ID of the movie that the shills are nuking
    int shillNukeFillAmount;
    int shillNukeBlendAmount;
} model_t;

/* random functions */
extern inline int randomInt(int lowerBound, int upperBound) { // random integer between lower and upper bound (inclusive)
    return (rand() % (upperBound - lowerBound + 1) + lowerBound);
}
extern inline double randomDouble(double lowerBound, double upperBound) { // random double between lower and upper bound
    return (rand() * (upperBound - lowerBound) / RAND_MAX + lowerBound); // probably works idk
}

/* loads the CSV files, initialises the lists, splits the train and test sets */
void modelInit(model_t *selfp) {
    model_t self = *selfp;
    // printf("random seed: %d\n", time(NULL));
    srand(1713216227); // set random seed
    self.movies = parseTSVAllowInts("MovieLensData/movies.tsv");
    printf("successfully loaded MovieLensData/movies.tsv\n");
    self.internalMovieID = list_init();
    for (int i = 0; i < self.movies -> data[0].r -> length; i++) {
        list_append(self.internalMovieID, self.movies -> data[0].r -> data[i], 'i');
    }
    self.trainRatings = parseCSVAllowInts("MovieLensData/ratings.csv");
    printf("successfully loaded MovieLensData/ratings.csv\n");
    /* print headers */
    // printRowCSV(self.movies, 0);
    // printRowCSV(self.trainRatings, 0);

    self.trainUsers = list_init();
    self.testUsers = list_init();
    self.shills = list_init();
    self.ranked = list_init();

    self.userReference = list_init();
    self.predictions = list_init();

    self.shillPush = 1; // we are shilling for toy story
    self.shillPushFillAmount = 10; // default 10
    self.shillPushBlendAmount = 10; // default 10

    self.shillNuke = 278; // nuke shawshank redemption
    self.shillNukeFillAmount = 10; // default 10
    self.shillNukeBlendAmount = 10; // default is 10

    /* split train and test sets */
    self.testSize = self.trainRatings -> data[0].r -> length / 10; // the test size is 1/10 the ratings set
    /* problem: if the test set is simply the last 1/10 of the ratings, then they are all the ratings for users > 550
    Ideally, we want the test ratings to be randomly distributed throughout the ratings set.
    The solution I have come up with is to delete testSize ratings from the trainRatings and add them to a separate list of testRatings
    */
    /* generate testSize random values */
    list_t *randomValues = list_init();
    for (int i = 0; i < self.testSize; i++) {
        list_append(randomValues, (unitype) randomInt(1, self.trainRatings -> data[0].r -> length - 1), 'i');
    }
    /* sort the random values */
    for (int i = 0; i < self.testSize; i++) {
        int select = i;
        for (int j = i; j < self.testSize; j++) {
            if (randomValues -> data[j].i > randomValues -> data[select].i) {
                select = j;
            }
        }
        /* swap i and select */
        int temp = randomValues -> data[i].i;
        randomValues -> data[i].i = randomValues -> data[select].i;
        randomValues -> data[select].i = temp;
    }
    // list_print(randomValues); // should be sorted biggest to smallest
    /* add from back to front to testRatings */
    self.testRatings = list_init();
    for (int i = 0; i < 4; i++) {
        /* there are 4 columns in the csv */
        list_append(self.testRatings, (unitype) list_init(), 'r');
    }
    for (int i = self.testSize - 1; i > -1; i--) {
        list_append(self.testRatings -> data[0].r, self.trainRatings -> data[0].r -> data[randomValues -> data[i].i], 'i');
        list_append(self.testRatings -> data[1].r, self.trainRatings -> data[1].r -> data[randomValues -> data[i].i], 'i');
        list_append(self.testRatings -> data[2].r, self.trainRatings -> data[2].r -> data[randomValues -> data[i].i], 'd');
        list_append(self.testRatings -> data[3].r, self.trainRatings -> data[3].r -> data[randomValues -> data[i].i], 'i');
    }
    /* delete from front to back from trainRatings */
    for (int i = 0; i < self.testSize; i++) {
        list_delete(self.trainRatings -> data[0].r, randomValues -> data[i].i);
        list_delete(self.trainRatings -> data[1].r, randomValues -> data[i].i);
        list_delete(self.trainRatings -> data[2].r, randomValues -> data[i].i);
        list_delete(self.trainRatings -> data[3].r, randomValues -> data[i].i);
    }
    printf("initialisation complete!\n");
    *selfp = self;
}

int convertToInternalMovieID(model_t *selfp, int movieID) {
    return list_find(selfp -> internalMovieID, (unitype) movieID, 'i');
}

/* populates the trainUsers list based on the trainRatings and the testUsers base don the testRatings */
void populateUsers(model_t *selfp) {
    model_t self = *selfp;
    /* trainUsers */
    int userID = 0;
    int lineNumber = 0;
    /* loop through ratings */
    while (lineNumber < self.trainRatings -> data[0].r -> length) {
        list_append(self.trainUsers, (unitype) list_init(), 'r');
        list_append(self.trainUsers -> data[userID].r, (unitype) userID, 'i'); // userID
        list_append(self.trainUsers -> data[userID].r, (unitype) list_init(), 'r'); // list of movieIDs (that this user has ranked)
        list_append(self.trainUsers -> data[userID].r, (unitype) list_init(), 'r'); // list of ratings
        if (userID != self.trainRatings -> data[0].r -> data[lineNumber].i) {
            lineNumber++;
        }
        while (lineNumber < self.trainRatings -> data[0].r -> length && userID == self.trainRatings -> data[0].r -> data[lineNumber].i) {
            /* add the movieID and rating to the user's rating lists */
            list_append(self.trainUsers -> data[userID].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.trainRatings -> data[1].r -> data[lineNumber].i), 'i');
            list_append(self.trainUsers -> data[userID].r -> data[2].r, self.trainRatings -> data[2].r -> data[lineNumber], 'd');
            lineNumber++;
        }
        userID++;
        // printf("user: %d, lineNumber %d\n", userID, lineNumber);
    }
    // list_print(self.trainUsers -> data[1].r);

    /* testUsers */
    userID = 0;
    lineNumber = 0;
    /* loop through testRatings */
    while (lineNumber < self.testRatings -> data[0].r -> length) {
        list_append(self.testUsers, (unitype) list_init(), 'r');
        list_append(self.testUsers -> data[userID].r, (unitype) userID, 'i'); // userID
        list_append(self.testUsers -> data[userID].r, (unitype) list_init(), 'r'); // list of movieIDs (that this user has ranked)
        list_append(self.testUsers -> data[userID].r, (unitype) list_init(), 'r'); // list of ratings
        if (userID != self.testRatings -> data[0].r -> data[lineNumber].i) {
            lineNumber++;
        }
        while (lineNumber < self.testRatings -> data[0].r -> length && userID == self.testRatings -> data[0].r -> data[lineNumber].i) {
            /* add the movieID and rating to the user's rating lists */
            list_append(self.testUsers -> data[userID].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.testRatings -> data[1].r -> data[lineNumber].i), 'i');
            list_append(self.testUsers -> data[userID].r -> data[2].r, self.testRatings -> data[2].r -> data[lineNumber], 'd');
            lineNumber++;
        }
        userID++;
        // printf("user: %d, lineNumber %d\n", userID, lineNumber);
    }
    // list_print(self.testUsers -> data[1].r);
    *selfp = self;
}

void printShillMessage(int numberOfShills, int shillStrategy) {
    switch (shillStrategy) {
        case SHILL_NAIVE_PUSH:
            printf("\nGenerated %d shills with the SHILL_NAIVE_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_RANDOM_FILL_PUSH:
            printf("\nGenerated %d shills with the SHILL_RANDOM_FILL_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_TARGETED_FILL_PUSH:
            printf("\nGenerated %d shills with the SHILL_TARGETED_FILL_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_RANDOM_BLEND_PUSH:
            printf("\nGenerated %d shills with the SHILL_RANDOM_BLEND_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_TARGETED_BLEND_PUSH:
            printf("\nGenerated %d shills with the SHILL_TARGETED_BLEND_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_NAIVE_NUKE:
            printf("\nGenerated %d shills with the SHILL_NAIVE_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_RANDOM_FILL_NUKE:
            printf("\nGenerated %d shills with the SHILL_RANDOM_FILL_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_TARGETED_FILL_NUKE:
            printf("\nGenerated %d shills with the SHILL_TARGETED_FILL_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_RANDOM_BLEND_NUKE:
            printf("\nGenerated %d shills with the SHILL_RANDOM_BLEND_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_TARGETED_BLEND_NUKE:
            printf("\nGenerated %d shills with the SHILL_TARGETED_BLEND_NUKE strategy\n", numberOfShills);
        break;
        default:
            printf("\nGenerated %d shills with an unknown shill strategy\n", numberOfShills);
        break;
    }
    
}

/* create a bunch of shills */
void populateShills(model_t *selfp, int numberOfShills, int shillStrategy) {
    model_t self = *selfp;
    // list_clear(self.shills); // allow multiple shills of different strategies
    int shillID = self.trainUsers -> length + self.shills -> length; // start shill userIDs at the last userID + 1
    for (int i = 0; i < numberOfShills; i++) {
        list_append(self.shills, (unitype) list_init(), 'r');
        list_append(self.shills -> data[i].r, (unitype) shillID, 'i'); // userID
        list_append(self.shills -> data[i].r, (unitype) list_init(), 'r'); // list of movieIDs (that this shill has ranked)
        list_append(self.shills -> data[i].r, (unitype) list_init(), 'r'); // list of ratings
        if (shillStrategy == SHILL_NAIVE_PUSH) {
            // the naive shill push only rates the shilled movie, and it rates it a 5/5
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
        }
        if (shillStrategy == SHILL_NAIVE_NUKE) {
            // the naive shill nuke only rates the self.shillNuke movie, and it rates it a 0/5
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
        }
        if (shillStrategy == SHILL_RANDOM_FILL_PUSH) {
            // the random shill push rates the shilled movie a 5/5 and x other random movies a 0/5 (x is determined by self.shillPushFillAmount)
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            int randomMovie;
            for (int j = 0; j < self.shillPushFillAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
            }
        }
        if (shillStrategy == SHILL_RANDOM_FILL_NUKE) {
            // the random shill nuke rates the nuked movie a 0/5 and x other random movies a 5/5 (x is determined by self.shillNukeFillAmount)
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
            int randomMovie;
            for (int j = 0; j < self.shillNukeFillAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            }
        }
        if (shillStrategy == SHILL_TARGETED_FILL_PUSH) {
            // this shill will rate the shilled movie a 5/5 and the x top movies a 0/5 (x is determined by self.shillPushFillAmount), top movies are decided by ARL
            // we must assume that the self.ranked list is already filled out
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_TARGETED_NUKE, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            for (int k = 0; k < self.shillPushFillAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
                    // printf("comapre: %lf\n", compare);
                    if (j != self.shillPush && list_count(topMovies, (unitype) j, 'i') == 0 && compare > maxAverageRank) {
                        maxAverageRank = compare;
                        topMovie = j;
                        ratingsCount = self.ranked -> data[j].r -> data[3].i;
                    }
                }
                list_append(topMovies, (unitype) topMovie, 'i');
                list_append(averageRank, (unitype) maxAverageRank, 'd');
                list_append(numberOfRatings, (unitype) ratingsCount, 'i');
            }
            for (int j = 0; j < self.shillPushFillAmount; j++) {
                // printf("shill %d nuking movie %d\n", i, convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i));
                // printf("shill %d nuking movie %d\n", i, self.movies -> data[0].r -> data[topMovies -> data[j].i].i);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i), 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        }
        if (shillStrategy == SHILL_TARGETED_FILL_NUKE) {
            // this shill will rate the nuked movie a 0/5 and the x top movies a 5/5 (x is determined by self.shillNukeFillAmount), top movies are decided by ARL
            // we must assume that the self.ranked list is already filled out
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_TARGETED_NUKE, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            for (int k = 0; k < self.shillNukeFillAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
                    // printf("comapre: %lf\n", compare);
                    if (j != self.shillNuke && list_count(topMovies, (unitype) j, 'i') == 0 && compare > maxAverageRank) {
                        maxAverageRank = compare;
                        topMovie = j;
                        ratingsCount = self.ranked -> data[j].r -> data[3].i;
                    }
                }
                list_append(topMovies, (unitype) topMovie, 'i');
                list_append(averageRank, (unitype) maxAverageRank, 'd');
                list_append(numberOfRatings, (unitype) ratingsCount, 'i');
            }
            for (int j = 0; j < self.shillNukeFillAmount; j++) {
                // printf("shill %d nuking movie %d\n", i, convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i));
                // printf("shill %d nuking movie %d\n", i, self.movies -> data[0].r -> data[topMovies -> data[j].i].i);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i), 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        }
        if (shillStrategy == SHILL_RANDOM_BLEND_PUSH) {
            // this shill will rate the shilled movie a 5/5 and x other random movies a 3/5 (x is determined by self.shillPushBlendAmount)
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            int randomMovie;
            for (int j = 0; j < self.shillPushBlendAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 3.0, 'd');
            }
        }
        if (shillStrategy == SHILL_RANDOM_BLEND_NUKE) {
            // this shill will rate the nuked movie a 0/5 and x other random movies a 3/5 (x is determined by self.shillNukeBlendAmount)
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
            int randomMovie;
            for (int j = 0; j < self.shillNukeBlendAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 3.0, 'd');
            }
        }
        if (shillStrategy == SHILL_TARGETED_BLEND_PUSH) {
            // this shill will rate the shilled movie a 5/5 and the x top movies a 3/5 (x is determined by self.shillPushBlendAmount), top movies are decided by ARL
            // we must assume that the self.ranked list is already filled out
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_BLEND_PUSH, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            for (int k = 0; k < self.shillPushBlendAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
                    // printf("comapre: %lf\n", compare);
                    if (j != self.shillPush && list_count(topMovies, (unitype) j, 'i') == 0 && compare > maxAverageRank) {
                        maxAverageRank = compare;
                        topMovie = j;
                        ratingsCount = self.ranked -> data[j].r -> data[3].i;
                    }
                }
                list_append(topMovies, (unitype) topMovie, 'i');
                list_append(averageRank, (unitype) maxAverageRank, 'd');
                list_append(numberOfRatings, (unitype) ratingsCount, 'i');
            }
            for (int j = 0; j < self.shillPushBlendAmount; j++) {
                // printf("shill %d nuking movie %d\n", i, convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i));
                // printf("shill %d nuking movie %d\n", i, self.movies -> data[0].r -> data[topMovies -> data[j].i].i);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i), 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 3.0, 'd');
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        }
        if (shillStrategy == SHILL_TARGETED_BLEND_NUKE) {
            // this shill will rate the nuked movie a 0/5 and the x top movies a 3/5 (x is determined by self.shillNukeBlendAmount), top movies are decided by ARL
            // we must assume that the self.ranked list is already filled out
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_BLEND_PUSH, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            for (int k = 0; k < self.shillNukeBlendAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
                    // printf("comapre: %lf\n", compare);
                    if (j != self.shillNuke && list_count(topMovies, (unitype) j, 'i') == 0 && compare > maxAverageRank) {
                        maxAverageRank = compare;
                        topMovie = j;
                        ratingsCount = self.ranked -> data[j].r -> data[3].i;
                    }
                }
                list_append(topMovies, (unitype) topMovie, 'i');
                list_append(averageRank, (unitype) maxAverageRank, 'd');
                list_append(numberOfRatings, (unitype) ratingsCount, 'i');
            }
            for (int j = 0; j < self.shillNukeBlendAmount; j++) {
                // printf("shill %d nuking movie %d\n", i, convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i));
                // printf("shill %d nuking movie %d\n", i, self.movies -> data[0].r -> data[topMovies -> data[j].i].i);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i), 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 3.0, 'd');
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        }
        shillID++;
    }
    printShillMessage(numberOfShills, shillStrategy);
    // list_print(self.shills);
    *selfp = self;
}

/* put shills in trainUser list */
void combineUsersAndShills(model_t *selfp) {
    model_t self = *selfp;
    for (int i = 0; i < self.shills -> length; i++) {
        list_append(self.trainUsers, (unitype) self.shills -> data[i].r, 'r');
    }
    *selfp = self;
}

/* calculates each movie's average rank */
void rankMovies(model_t *selfp) {
    model_t self = *selfp;
    list_clear(self.ranked);
    list_t *tempSumRanks = list_init();
    list_t *tempNumRatings = list_init();

    list_append(self.ranked, (unitype) list_init(), 'r'); // 0th movie has no data
    // list_append(self.ranked -> data[0].r, (unitype) 0, 'i'); // movieID
    // list_append(self.ranked -> data[0].r, (unitype) "nothing", 's'); // movieName
    // list_append(self.ranked -> data[0].r, (unitype) (double) 0.0, 'd'); // average rank (unknown)
    // list_append(self.ranked -> data[0].r, (unitype) 0, 'i'); // number of ratings (unknown)
    list_append(tempSumRanks, (unitype) (double) 0.0, 'd');
    list_append(tempNumRatings, (unitype) 0, 'i'); 
    // printf("number of movies: %d\n", self.movies -> data[0].r -> length);
    for (int i = 1; i < self.movies -> data[0].r -> length; i++) {
        list_append(self.ranked, (unitype) list_init(), 'r');
        list_append(self.ranked -> data[i].r, self.movies -> data[0].r -> data[i], 'i'); // movieID
        list_append(self.ranked -> data[i].r, self.movies -> data[1].r -> data[i], 's'); // movieName
        list_append(self.ranked -> data[i].r, (unitype) (double) 0.0, 'd'); // average rank (unknown)
        list_append(self.ranked -> data[i].r, (unitype) 0, 'i'); // number of ratings (unknown)

        list_append(tempSumRanks, (unitype) (double) 0.0, 'd');
        list_append(tempNumRatings, (unitype) 0, 'i'); 
    }
    // list_print(self.ranked -> data[0].r);
    for (int i = 1; i < self.trainUsers -> length; i++) {
        /* loop over all users */
        // printf("user: %d\n", self.trainUsers -> data[i].r -> data[0].i);
        for (int j = 0; j < self.trainUsers -> data[i].r -> data[1].r -> length; j++) {
            /* loop over all movies ranked by this user, add the rank to sumRanks and increment numRatings by 1 */
            // printf("user %d ranked movie %d a %lf\n", self.trainUsers -> data[i].r -> data[0].i, self.trainUsers -> data[i].r -> data[1].r -> data[j].i, self.trainUsers -> data[i].r -> data[2].r -> data[j].d);
            tempSumRanks -> data[self.trainUsers -> data[i].r -> data[1].r -> data[j].i].d += self.trainUsers -> data[i].r -> data[2].r -> data[j].d;
            tempNumRatings -> data[self.trainUsers -> data[i].r -> data[1].r -> data[j].i].i++;
        }
    }
    for (int i = 1; i < self.movies -> data[0].r -> length; i++) {
        /* fill in number of ratings */
        self.ranked -> data[i].r -> data[3] = tempNumRatings -> data[i];
        /* calculate average rank as sum of rankings / number of rankings */
        self.ranked -> data[i].r -> data[2].d = tempSumRanks -> data[i].d / tempNumRatings -> data[i].i;
    }
    list_free(tempSumRanks);
    list_free(tempNumRatings);
    // list_print(self.ranked -> data[0].r);
    *selfp = self;
}

/*
displays top x movies given a ranked list
Calculated by average rating * log(numberOfRatings + 1)
the log base is 10

This is a made up metric, but it makes some sense to gauge how well liked a movie is.
I just want to see what the effect of the shills are on this

I'm calling the metric ARL by the way, for (average rating) * log
Just because I think it's funny how many cryptic abbreviations the recommenders systems field has
Maybe they just inherited them from the statisticians

Note: This only takes the trainset into account
*/
void displayTopMovies(model_t *selfp, list_t *rankList, int topX) {
    list_t *topMovies = list_init();
    list_t *averageRank = list_init();
    list_t *numberOfRatings = list_init();
    // list_print(rankList -> data[278].r);
    for (int j = 0; j < topX; j++) {
        double maxAverageRank = -1;
        int ratingsCount;
        int topMovie;
        for (int i = 1; i < rankList -> length; i++) {
            double compare = rankList -> data[i].r -> data[2].d * log(rankList -> data[i].r -> data[3].i + 1) / log(10);
            if (list_count(topMovies, (unitype) i, 'i') == 0 && compare > maxAverageRank) {
                // printf("compare: %d %lf %lf %d\n", i, compare, rankList -> data[i].r -> data[2].d, rankList -> data[i].r -> data[3].i);
                maxAverageRank = compare;
                topMovie = i;
                ratingsCount = rankList -> data[i].r -> data[3].i;
            }
        }
        list_append(topMovies, (unitype) topMovie, 'i');
        list_append(averageRank, (unitype) maxAverageRank, 'd');
        list_append(numberOfRatings, (unitype) ratingsCount, 'i');
    }
    for (int i = 0; i < topX; i++) {
        printf("%d. (ID: %d, InternalID: %d) %s, score: %0.2lf with %d ratings\n", i + 1, selfp -> movies -> data[0].r -> data[topMovies -> data[i].i].i, convertToInternalMovieID(selfp, selfp -> movies -> data[0].r -> data[topMovies -> data[i].i].i), selfp -> movies -> data[1].r -> data[topMovies -> data[i].i].s, averageRank -> data[i].d, numberOfRatings -> data[i].i);
    }
    list_free(topMovies);
    list_free(averageRank);
    list_free(numberOfRatings);
}

/* generates the userReference list so that I don't have to search through the list a billion times. */
void generateUserReferenceCache(model_t *selfp) {
    model_t self = *selfp;
    list_clear(self.userReference);
    list_append(self.userReference, (unitype) list_init(), 'r'); // there is no user0
    for (int user = 1; user < self.trainUsers -> length; user++) {
        list_append(self.userReference, (unitype) list_init(), 'r');
        for (int movie = 0; movie < self.movies -> data[0].r -> length; movie++) {
            /* could be optimised but it doesn't even take that long anyway */
            list_append(self.userReference -> data[user].r, (unitype) list_find(self.trainUsers -> data[user].r -> data[1].r, (unitype) movie, 'i'), 'i');
        }
    }
    printf("Generated UserReferenceCache\n");
    *selfp = self;
}

/* generate userUser predictions - https://www.geeksforgeeks.org/user-based-collaborative-filtering This takes about 20 seconds to complete on my machine */
void userUser(model_t *selfp) {
    model_t self = *selfp;
    /* the rating means for each movie are stored in self.ranked -> data[interalID].r -> data[2].d */
    /* find similarity of each user to each other */
    /* calculate user averages */
    list_t *userAverages = list_init();
    list_append(userAverages, (unitype) 0.0, 'd'); // user 0 does not exist
    for (int user = 1; user < self.trainUsers -> length; user++) {
        /* calculate the average of this user */
        double userAverage = 0.0;
        for (int userRatings = 0; userRatings < self.trainUsers -> data[user].r -> data[2].r -> length; userRatings++) {
            userAverage += self.trainUsers -> data[user].r -> data[2].r -> data[userRatings].d;
        }
        userAverage /= self.trainUsers -> data[user].r -> data[2].r -> length;
        list_append(userAverages, (unitype) userAverage, 'd');
    }

    list_t *similarity = list_init(); // spec: [targetUser, [user1, user2, user3, user4, ...]]

    list_append(similarity, (unitype) list_init(), 'r'); // 0th user has no data
    list_append(similarity -> data[0].r, (unitype) 0, 'i');
    for (int user1 = 1; user1 < self.trainUsers -> length; user1++) { // start at 1 since user 0 does not exist
        /* loop over users*/
        list_append(similarity, (unitype) list_init(), 'r');
        list_append(similarity -> data[user1].r, (unitype) user1, 'i');
        list_append(similarity -> data[user1].r, (unitype) list_init(), 'r');
        list_append(similarity -> data[user1].r -> data[1].r, (unitype) 0.0, 'd'); // user 0 does not exist
        for (int user2 = 1; user2 < self.trainUsers -> length; user2++) {
            /* loop over users, again! */
            double similarityToUser1 = 0.0;
            /* calculate similarity between user1 and user2 */
            double numerator = 0.0;
            double user1Denominator = 0.0;
            double user2Denominator = 0.0;
            for (int movieID = 0; movieID < self.movies -> data[0].r -> length; movieID++) {
                /* loop over all movies */
                /* determine if user1 or user2 rated the movie */
                int user1RatedMovie = self.userReference -> data[user1].r -> data[movieID].i;
                int user2RatedMovie = self.userReference -> data[user2].r -> data[movieID].i;
                if (user1RatedMovie > -1) {
                    if (user2RatedMovie > -1) {
                        // both users 1 and 2 rated the movie
                        numerator += (self.trainUsers -> data[user1].r -> data[2].r -> data[user1RatedMovie].d - userAverages -> data[user1].d) * (self.trainUsers -> data[user2].r -> data[2].r -> data[user2RatedMovie].d - userAverages -> data[user2].d);
                        user1Denominator += (self.trainUsers -> data[user1].r -> data[2].r -> data[user1RatedMovie].d - userAverages -> data[user1].d) * (self.trainUsers -> data[user1].r -> data[2].r -> data[user1RatedMovie].d - userAverages -> data[user1].d);
                        user2Denominator += (self.trainUsers -> data[user2].r -> data[2].r -> data[user2RatedMovie].d - userAverages -> data[user2].d) * (self.trainUsers -> data[user2].r -> data[2].r -> data[user2RatedMovie].d - userAverages -> data[user2].d);
                    } else {
                        // only user1 rated the movie
                        // numerator does not change
                        user1Denominator += (self.trainUsers -> data[user1].r -> data[2].r -> data[user1RatedMovie].d - userAverages -> data[user1].d) * (self.trainUsers -> data[user1].r -> data[2].r -> data[user1RatedMovie].d - userAverages -> data[user1].d);
                        // user2Denominator does not change
                    }
                } else if (user2RatedMovie > -1) {
                    // only user2 rated the movie
                    // numerator does not change
                    // user1Denominator does not change
                    user2Denominator += (self.trainUsers -> data[user2].r -> data[2].r -> data[user2RatedMovie].d - userAverages -> data[user2].d) * (self.trainUsers -> data[user2].r -> data[2].r -> data[user2RatedMovie].d - userAverages -> data[user2].d);
                } else {
                    // neither user rated the movie
                    // numerator does not change
                    // denominator does not change
                }
            }
            user1Denominator = sqrt(user1Denominator);
            user2Denominator = sqrt(user2Denominator);
            similarityToUser1 = numerator / (user1Denominator * user2Denominator);
            // quick nan check
            if (similarityToUser1 < 100000) {
                list_append(similarity -> data[user1].r -> data[1].r, (unitype) similarityToUser1, 'd');
            } else {
                // nan values will say "no, this value is not under 100000" but every in bounds value will be
                list_append(similarity -> data[user1].r -> data[1].r, (unitype) 0.0, 'd');
            }
        }
    }
    /* to access similarity between users a and b, do similarity -> data[a].r -> data[1].r -> data[b], this also works in reverse */
    /* fill in unknown ratings using the similarity matrix */
    list_append(self.predictions, (unitype) list_init(), 'r'); // user 0 does not exist
    list_append(self.predictions -> data[0].r, (unitype) 0, 'i');
    list_append(self.predictions -> data[0].r, (unitype) list_init(), 'r');
    for (int user = 1; user < self.trainUsers -> length; user++) {
        list_append(self.predictions, (unitype) list_init(), 'r');
        list_append(self.predictions -> data[user].r, (unitype) user, 'i');
        list_append(self.predictions -> data[user].r, (unitype) list_init(), 'r');
        for (int movie = 0; movie < self.movies -> data[0].r -> length; movie++) {
            /* calculate prediction */
            int userRatedMovie = self.userReference -> data[user].r -> data[movie].i;
            if (userRatedMovie > -1) {
                /* user has already rated the movie explicitly */
                list_append(self.predictions -> data[user].r -> data[1].r, self.trainUsers -> data[user].r -> data[2].r -> data[userRatedMovie], 'd');
            } else {
                /* user has not rated movie explicitly, run the formula! */
                double predictedRating = userAverages -> data[user].d;
                double numerator = 0.0;
                double denominator = 0.0;
                for (int loopUser = 1; loopUser < self.trainUsers -> length; loopUser++) {
                    /* check if loopUser has rated this movie */
                    int loopUserRatedMovie = self.userReference -> data[loopUser].r -> data[movie].i;
                    if (loopUserRatedMovie > -1) {
                        /* loop user rated the movie */
                        numerator += similarity -> data[user].r -> data[1].r -> data[loopUser].d * self.trainUsers -> data[loopUser].r -> data[2].r -> data[loopUserRatedMovie].d;
                    }
                    denominator += fabs(similarity -> data[user].r -> data[1].r -> data[loopUser].d);
                }
                predictedRating += numerator / denominator;
                if (predictedRating < 100000) {
                    /* not nan, it is good */
                    list_append(self.predictions -> data[user].r -> data[1].r, (unitype) predictedRating, 'd');
                } else {
                    /* it is nan, just make the prediction the average */
                    list_append(self.predictions -> data[user].r -> data[1].r, userAverages -> data[user], 'd');
                }
            }
        }
    }
    // list_print(self.predictions -> data[1].r); // print out user1's predictions
    list_free(userAverages);
    list_free(similarity);
    /* to access prediction of a user to a movie, do self.prediction -> data[user].r -> data[1].r -> data[movie].d */
    *selfp = self;
}

/* generate itemItem predictions */
void itemItem(model_t *selfp) {

}

/* generate SVD predictions */
void SVD(model_t *selfp) {

}

/* generate predictions according to the X algorithm (what algorithm?) */
void generatePredictions(model_t *selfp, int predictionMethod) {
    list_clear(selfp -> predictions);
    switch (predictionMethod) {
        case PREDICTION_USER_USER:
            userUser(selfp);
        break;
        case PREDICTION_ITEM_ITEM:
            itemItem(selfp);
        break;
        case PREDICTION_SVD:
            SVD(selfp);
        break;
        default:
            printf("generatePredictions: predictionMethod %d not recognised\n", predictionMethod);
        break;
    }
}

/* Evaluate predictions with MAE */
double MAE(model_t *selfp) {
    model_t self = *selfp;
    double loss = 0.0;
    /* we're only going to loop through the users in the testset, the testUsers */
    for (int user = 1; user < self.testUsers -> length; user++) {
        /* loop through testUser's ratings */
        for (int movie = 0; movie < self.testUsers -> data[user].r -> data[1].r -> length; movie++) {
            /* compare predicted rating with real rating */
            int movieID = self.testUsers -> data[user].r -> data[1].r -> data[movie].i;
            double predictedRating = self.predictions -> data[user].r -> data[1].r -> data[movieID].d;
            double realRating = self.testUsers -> data[user].r -> data[2].r -> data[movie].d;
            if (!(predictedRating < 100000)) {
                printf("predicted rating nan for user %d and movie %d\n", user, movieID);
            }
            if (!(realRating < 100000)) {
                printf("real rating nan for user %d and movie %d\n", user, movieID);
            }
            loss += fabs(predictedRating - realRating);
        }
    }
    loss /= (self.testRatings -> data[0].r -> length - 1);
    *selfp = self;
    return loss;
}

/* Evaluate predictions with RMSE */
double RMSE(model_t *selfp) {
    model_t self = *selfp;
    double loss = 0.0;
    /* we're only going to loop through the users in the testset, the testUsers */
    for (int user = 1; user < self.testUsers -> length; user++) {
        /* loop through testUser's ratings */
        for (int movie = 0; movie < self.testUsers -> data[user].r -> data[1].r -> length; movie++) {
            /* compare predicted rating with real rating */
            int movieID = self.testUsers -> data[user].r -> data[1].r -> data[movie].i;
            // list_print(self.testUsers -> data[user].r -> data[1].r);
            // printf("movieID: %d\n", movieID);
            // list_print(self.predictions -> data[user].r);
            double predictedRating = self.predictions -> data[user].r -> data[1].r -> data[movieID].d;
            // list_print(self.testUsers -> data[user].r -> data[2].r);
            // printf("predicted: %lf\n", predictedRating);
            double realRating = self.testUsers -> data[user].r -> data[2].r -> data[movie].d;
            // printf("real: %lf\n", realRating);
            if (!(predictedRating < 100000)) {
                printf("predicted rating nan for user %d and movie %d\n", user, movieID);
            }
            if (!(realRating < 100000)) {
                printf("real rating nan for user %d and movie %d\n", user, movieID);
            }
            loss += (predictedRating - realRating) * (predictedRating - realRating);
        }
        // printf("loss: %lf\n", loss);
    }
    loss /= (self.testRatings -> data[0].r -> length - 1);
    loss = sqrt(loss);
    *selfp = self;
    return loss;
}

/* Evaluate predictions with ndcg at k */
double ndcg(model_t *selfp, int atK) {
    model_t self = *selfp;
    *selfp = self;
    return 0.0;
}

/* the SHE (shill effectiveness) metric counts the number of times that the shilled item appears in test users recommendation list of size topX */
int SHE(model_t *selfp, int topX, char pushOrNuke) {
    model_t self = *selfp;
    int result = 0;
    /* for each test user */
    for (int user = 1; user < self.testUsers -> length; user++) {
        /* gather topX predicted scores */
        list_t *recList = list_init();
        for (int i = 0; i < topX; i++) {
            int addToList = 0;
            double maxPredicted = -100000;
            for (int movieID = 0; movieID < self.movies -> data[0].r -> length; movieID++) {
                double predictedScore = self.predictions -> data[user].r -> data[1].r -> data[movieID].d;
                if (list_count(recList, (unitype) movieID, 'i') == 0 && predictedScore > maxPredicted) {
                    maxPredicted = predictedScore;
                    addToList = movieID;
                }
            }
            list_append(recList, (unitype) addToList, 'i');
        }
        // list_print(recList);
        /* check if/where shilled item is in recommendation list */
        int position;
        if (pushOrNuke) {
            position = list_find(recList, (unitype) self.shillNuke, 'i');
        } else {
            position = list_find(recList, (unitype) self.shillPush, 'i');
        }
        if (position > -1) {
            result += topX - position;
        }
        list_free(recList);
    }
    *selfp = self;
    return result;
}

int main(int argc, char  *argv[]) {
    /* quick testing binary search */
    // list_t *test = list_init();
    // for (int i = 0; i < 100; i++) {
    //     list_append(test, (unitype) i, 'i');
    // }
    // list_delete(test, 20);
    // for (int i = 0; i < 100; i++) {
    //     printf("search for %d: %d\n", i, list_find_binary(test, i));
    // }

    model_t self;
    modelInit(&self);
    /* identify users */
    populateUsers(&self);
    generateUserReferenceCache(&self); // cached for speed
    generatePredictions(&self, PREDICTION_USER_USER);
    /* evaluate */
    printf("\nMAE Evaluation (No Shills): %lf\n", MAE(&self));
    printf("RMSE Evaluation (No Shills): %lf\n", RMSE(&self));
    // printf("Number of times \"Toy Story\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 0));
    printf("Number of times \"Shawshank Redemption\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 1));
    /* rank movies */
    rankMovies(&self);
    printf("\nTop 10 Movies by ARL (No Shills):\n");
    displayTopMovies(&self, self.ranked, 10);
    /* generate shills */
    // populateShills(&self, 100, SHILL_NAIVE_PUSH); // 140 shills are needed at minimum to get toy story to the top
    // populateShills(&self, 100, SHILL_RANDOM_FILL_PUSH); // 140 shills are a maximum guarentee, it could be less based on random chance (with shillPushFillAmount = 10)
    // populateShills(&self, 100, SHILL_TARGETED_FILL_PUSH); // just 46 targeted nuke shills are enough to get toy story to the top (with shillPushFillAmount = 10)
    // populateShills(&self, 100, SHILL_RANDOM_BLEND_PUSH);
    // populateShills(&self, 100, SHILL_TARGETED_BLEND_PUSH);

    // populateShills(&self, 100, SHILL_NAIVE_NUKE);
    // populateShills(&self, 100, SHILL_RANDOM_FILL_NUKE);
    // populateShills(&self, 100, SHILL_TARGETED_FILL_NUKE);
    // populateShills(&self, 100, SHILL_RANDOM_BLEND_NUKE);
    populateShills(&self, 100, SHILL_TARGETED_BLEND_NUKE);
    /* combine users and shills */
    combineUsersAndShills(&self);
    /* generate predictions */
    generateUserReferenceCache(&self); // cached for speed
    generatePredictions(&self, PREDICTION_USER_USER);
    /* evaluate */
    printf("\nMAE Evaluation (After Shills): %lf\n", MAE(&self));
    printf("RMSE Evaluation (After Shills): %lf\n", RMSE(&self));
    // printf("Number of times \"Toy Story\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 0));
    printf("Number of times \"Shawshank Redemption\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 1));
    /* rank movies */
    rankMovies(&self);
    printf("\nTop 10 Movies by ARL (After Shills):\n");
    displayTopMovies(&self, self.ranked, 10);
}