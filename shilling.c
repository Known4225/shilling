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

Testing with:
 - Fixed randomness (using randPredictive with predSrand = 1)
 - shillPush = 1
 - shillNuke = 278
 - shillRandomAmount, shillPopularAmount = 10
 - shillProbeAmount = 3

Shill Strategy                  Algorithm      SHE (no shills)     SHE (with 100 shills)    ShillPush    ShillNuke       Effect
SHILL_NAIVE_PUSH                user-user           122                    122                  1           NA           None
SHILL_RANDOM_PUSH               user-user           88                     111                  1           NA           Correct
SHILL_LOVE_HATE_PUSH            user-user           97                     134                  1           NA           Correct
SHILL_BANDWAGON_PUSH            user-user           110                    341                  1           NA           Correct
SHILL_POPULAR_PUSH              user-user           118                     97                  1           NA           Opposite
SHILL_REVERSE_BANDWAGON_PUSH    user-user           NA                      NA                  NA          NA           NA
SHILL_PROBE_PUSH                user-user           NA                      NA                  NA          NA           NA

SHILL_NAIVE_NUKE                user-user           639                    639                  NA          278          None
SHILL_RANDOM_NUKE               user-user           647                    782                  NA          278          Opposite
SHILL_LOVE_HATE_NUKE            user-user           722                    853                  NA          278          Opposite
SHILL_BANDWAGON_NUKE            user-user           615                    330                  NA          278          Correct
SHILL_POPULAR_NUKE              user-user           656                    714                  NA          278          Opposite
SHILL_REVERSE_BANDWAGON_PUSH    user-user           NA                      NA                  NA          NA           NA
SHILL_PROBE_PUSH                user-user           NA                      NA                  NA          NA           NA

Analysis:

*/
#include "include/csvParser.h"
#include <math.h>
#include <time.h>

/* shill strategy macros */
#define SHILL_NAIVE_PUSH                1
#define SHILL_RANDOM_PUSH               2
#define SHILL_LOVE_HATE_PUSH            3
#define SHILL_BANDWAGON_PUSH            4
#define SHILL_POPULAR_PUSH              5
#define SHILL_REVERSE_BANDWAGON_PUSH    6 // this is not implented. The reverse bandwagon is only a nuking strategy
#define SHILL_PROBE_PUSH                7
#define SHILL_SEGMENT_PUSH              8 // not implemented

#define SHILL_NAIVE_NUKE               11
#define SHILL_RANDOM_NUKE              12
#define SHILL_LOVE_HATE_NUKE           13
#define SHILL_BANDWAGON_NUKE           14
#define SHILL_POPULAR_NUKE             15
#define SHILL_REVERSE_BANDWAGON_NUKE   16
#define SHILL_PROBE_NUKE               17
#define SHILL_SEGMENT_NUKE             18 // not implemented

/* 
Brief description of different attack types:
SHILL_NAIVE - the naive shill simply rates the pushed/nuked item and nothing else. A push shill rates its item a 5/5 and a nuke shill rates it a 0/5. The push/nuked item are determined by shillPush and shillNuke (in the model)
SHILL_RANDOM - the random shill rates the pushed/nuked item as well as x other random items. The rating of the random items is random as well, but centered at the global mean (uniformly distributed as globalMean ± 1), x is determined by shillRandomAmount
SHILL_LOVE_HATE - the love/hate shill rates the pushed/nuked item as well as x other random items. In the case of a push shill the random items are rated 0/5 and in the case of a nuke 5/5, x is determined by shillRandomAmount
SHILL_BANDWAGON - the bandwagon shill rates the pushed/nuked item as well as y + x other items. The top y items (top determined by ARL, y set by shillPopularAmount) are rated 5/5 and the other x items are rated randomly, x is determined by shillRandomAmount
SHILL_POPULAR - the popular shill rates the pushed/nuked item as well as y + x other items. The top y items (top determined by ARL, y set by shillPopularAmount) as well as the next top x items are rated either 0/5 or 1/5 depending on whether the average for that item is above or below the total average
SHILL_REVERSE_BANDWAGON - the reverse bandwagon shill rates the nuked item as well as z other items. All z items are rated 0/5, z items are ranked by taking the number of votes divided by the average rating squared.
SHILL_PROBE - the probe shill first rates n items randomly (n determined by shillProbeAmount). It then uses recommendation predictions to rate x items (rates them the predicted score ± 0.1). It then rates the push/nuked item.
SHILL_SEGMENT - the segment shill is not implemented

These are taken from section 12.3 of "Recommender Systems: The Textbook" by Charu C. Aggarwal
*/

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
    double globalAverageRating; // global average for all ratings

    int shillPush; // the ID of the movie that the shills are pushing
    int shillPushRandomAmount; // in shills that push and rate random movies, this is how many random movies they will rate
    int shillPushPopularAmount; // in shills that push and rate popular movies, this is how many of the top movies they will rate
    int shillPushProbeAmount; // in probe shills that push, the number of random items they rate before probing the recommendation system

    int shillNuke; // the ID of the movie that the shills are nuking
    int shillNukeRandomAmount; // in shills that nuke and rate random movies, this is how many random movies they will rate
    int shillNukePopularAmount; // in shills that nuke and rate popular movies, this is how many of the top movies they will rate
    int shillNukeProbeAmount; // in probe shills that push, the number of random items they rate before probing the recommendation system
} model_t;

/* random functions */
unsigned long predSrand = 100; // starting value for randPredictive
int randPredictive(void) { // https://stackoverflow.com/questions/65980429/same-srand-seeds-produces-different-values-on-different-computers
    predSrand = predSrand * 1103515245 + 12345;
    return (unsigned int) (predSrand / 65536) % 32768;
}

extern inline int randomInt(int lowerBound, int upperBound) { // random integer between lower and upper bound (inclusive)
    return (randPredictive() % (upperBound - lowerBound + 1) + lowerBound);
}
extern inline double randomDouble(double lowerBound, double upperBound) { // random double between lower and upper bound
    return (randPredictive() * (upperBound - lowerBound) / RAND_MAX + lowerBound); // probably works idk
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
    // printRowCSV(self.trainRatings, 1);
    // list_print(self.trainRatings -> data[1].r);
    // printf("number of ratings: %d\n", self.trainRatings -> data[0].r -> length);

    self.trainUsers = list_init();
    self.testUsers = list_init();
    self.shills = list_init();
    self.ranked = list_init();
    self.globalAverageRating = 3.0;

    self.userReference = list_init();
    self.predictions = list_init();

    self.shillPush = 2; // we are shilling for jumanji (1995)
    self.shillPushRandomAmount = 15; // default 15
    self.shillPushPopularAmount = 15; // default 15
    self.shillPushProbeAmount = 3; // default is 3

    self.shillNuke = 255; // we are nuking Star Wars: Episode IV - A New Hope (1977)
    self.shillNukeRandomAmount = 15; // default 15
    self.shillNukePopularAmount = 15; // default is 15
    self.shillNukeProbeAmount = 3; // default is 3

    /* split train and test sets */
    self.testSize = self.trainRatings -> data[0].r -> length / 10; // the test size is 1/10 the ratings set
    /* problem: if the test set is simply the last 1/10 of the ratings, then they are all the ratings for users > 550
    Ideally, we want the test ratings to be randomly distributed throughout the ratings set.
    The solution I have come up with is to delete testSize ratings from the trainRatings and add them to a separate list of testRatings
    */
    /* generate testSize random values */
    list_t *randomValues = list_init();
    for (int i = 0; i < self.testSize; i++) {
        // if (i < 100)
        //     printf("randomInt: %d\n", randomInt(1, self.trainRatings -> data[0].r -> length - 1));
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
    list_free(randomValues);
    printf("initialisation complete!\n");
    *selfp = self;
}

void model_free(model_t *selfp, char debug) {
    list_free(selfp -> movies);
    if (debug)
        printf("freed movies\n");
    list_free(selfp -> trainRatings);
    if (debug)
        printf("freed trainRatings\n");
    list_free(selfp -> testRatings);
    if (debug)
        printf("freed testRatings\n");
    list_free(selfp -> internalMovieID);
    if (debug)
        printf("freed internalMovieID\n");
    list_free(selfp -> trainUsers);
    if (debug)
        printf("freed trainUsers\n");
    list_free(selfp -> testUsers);
    if (debug)
        printf("freed testUsers\n");
    list_free(selfp -> shills);
    if (debug)
        printf("freed shills\n");
    list_free(selfp -> userReference);
    if (debug)
        printf("freed userReference\n");
    list_free(selfp -> predictions);
    if (debug)
        printf("freed predictions\n");
    list_free(selfp -> ranked);
    if (debug)
        printf("freed ranked\n");
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
        case SHILL_RANDOM_PUSH:
            printf("\nGenerated %d shills with the SHILL_RANDOM_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_LOVE_HATE_PUSH:
            printf("\nGenerated %d shills with the SHILL_LOVE_HATE_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_BANDWAGON_PUSH:
            printf("\nGenerated %d shills with the SHILL_BANDWAGON_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_POPULAR_PUSH:
            printf("\nGenerated %d shills with the SHILL_POPULAR_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_REVERSE_BANDWAGON_PUSH:
            printf("\nGenerated %d shills with the SHILL_REVERSE_BANDWAGON_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_PROBE_PUSH:
            printf("\nGenerated %d shills with the SHILL_PROBE_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_SEGMENT_PUSH:
            printf("\nGenerated %d shills with the SHILL_SEGMENT_PUSH strategy\n", numberOfShills);
        break;
        case SHILL_NAIVE_NUKE:
            printf("\nGenerated %d shills with the SHILL_NAIVE_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_RANDOM_NUKE:
            printf("\nGenerated %d shills with the SHILL_RANDOM_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_LOVE_HATE_NUKE:
            printf("\nGenerated %d shills with the SHILL_LOVE_HATE_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_BANDWAGON_NUKE:
            printf("\nGenerated %d shills with the SHILL_BANDWAGON_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_POPULAR_NUKE:
            printf("\nGenerated %d shills with the SHILL_POPULAR_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_REVERSE_BANDWAGON_NUKE:
            printf("\nGenerated %d shills with the SHILL_REVERSE_BANDWAGON_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_PROBE_NUKE:
            printf("\nGenerated %d shills with the SHILL_PROBE_NUKE strategy\n", numberOfShills);
        break;
        case SHILL_SEGMENT_NUKE:
            printf("\nGenerated %d shills with the SHILL_SEGMENT_NUKE strategy\n", numberOfShills);
        break;
        default:
            printf("\nError: unknown shill strategy\n");
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
            // the naive push only rates the self.shillPush movie, and it rates it a 5/5
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
        } else if (shillStrategy == SHILL_NAIVE_NUKE) {
            // the naive nuke only rates the self.shillNuke movie, and it rates it a 0/5
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
        } else if (shillStrategy == SHILL_RANDOM_PUSH) {
            // the random push rates the shilled movie a 5/5 and x other random movies the global average plus or minus 1 (x is determined by self.shillPushRandomAmount)
            // rankMovies must be called prior to populateShills to determine the global average rating
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_RANDOM_PUSH, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');

            int randomMovie;
            for (int j = 0; j < self.shillPushRandomAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) (self.globalAverageRating + randomDouble(-1, 1)), 'd');
            }
        } else if (shillStrategy == SHILL_RANDOM_NUKE) {
            // the random nuke rates the nuked movie a 0/5 and x other random movies the global average plus or minus 1 (x is determined by self.shillNukeRandomAmount)
            // rankMovies must be called prior to populateShills to determine the global average rating
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_RANDOM_NUKE, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');

            int randomMovie;
            for (int j = 0; j < self.shillNukeRandomAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) (self.globalAverageRating + randomDouble(-1, 1)), 'd');
            }
        } else if (shillStrategy == SHILL_LOVE_HATE_PUSH) {
            // the love/hate push rates the pushed item fa 5/5 and x other random items a 0/5, x is determined by shillRandomAmount
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');

            int randomMovie;
            for (int j = 0; j < self.shillPushRandomAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
            }
        } else if (shillStrategy == SHILL_LOVE_HATE_NUKE) {
            // the love/hate nuke rates the nuked item a 0/5 and x other random items a 5/5, x is determined by shillRandomAmount
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');

            int randomMovie;
            for (int j = 0; j < self.shillNukeRandomAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            }
        } else if (shillStrategy == SHILL_BANDWAGON_PUSH) {
            // the bandwagon push rates the pushed item a 5/5 and y + x other items. The top y items (top determined by ARL, y set by shillPopularAmount) are rated 5/5 and the other x items are rated randomly, x is determined by shillRandomAmount
            // rankMovies must be called prior to populateShills to determine ARL positions
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_BANDWAGON_PUSH, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            int randomMovie;
            for (int j = 0; j < self.shillPushRandomAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) (double) (self.globalAverageRating + randomDouble(-1, 1)), 'd');
            }
            for (int k = 0; k < self.shillPushPopularAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
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
            for (int j = 0; j < self.shillPushPopularAmount; j++) {
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i), 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        } else if (shillStrategy == SHILL_BANDWAGON_NUKE) {
            // the bandwagon nuke rates the nuked item a 0/5 and y + x other items. The top y items (top determined by ARL, y set by shillPopularAmount) are rated 5/5 and the other x items are rated randomly, x is determined by shillRandomAmount
            // rankMovies must be called prior to populateShills to determine ARL positions
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_BANDWAGON_NUKE, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            int randomMovie;
            for (int j = 0; j < self.shillNukeRandomAmount; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') == -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                // printf("shill %d rating movie %d\n", i, randomMovie);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) (double) (self.globalAverageRating + randomDouble(-1, 1)), 'd');
            }
            for (int k = 0; k < self.shillNukePopularAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
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
            for (int j = 0; j < self.shillNukePopularAmount; j++) {
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i), 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        } else if (shillStrategy == SHILL_POPULAR_PUSH) {
            // the popular push rates the pushed item a 5/5 as y + x other items. The top y items (top determined by ARL, y set by shillPopularAmount) as well as the next top x random items are rated either 0/5 or 1/5 depending on whether the average for that item is above or below the total average
            // rankMovies must be called prior to populateShills to determine ARL positions
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_POPULAR_PUSH, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            for (int k = 0; k < self.shillPushPopularAmount + self.shillPushRandomAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
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
            for (int j = 0; j < self.shillPushPopularAmount + self.shillPushRandomAmount; j++) {
                int movieID = convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) movieID, 'i');
                if (self.ranked -> data[movieID].r -> data[2].d > self.globalAverageRating) {
                    list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 1.0, 'd');
                } else {
                    list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
                }
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        } else if (shillStrategy == SHILL_POPULAR_NUKE) {
            // the popular push rates the pushed item a 5/5 as y + x other items. The top y items (top determined by ARL, y set by shillPopularAmount) as well as the next top x random items are rated either 0/5 or 1/5 depending on whether the average for that item is above or below the total average
            // rankMovies must be called prior to populateShills to determine ARL positions
            if (self.ranked -> length == 0) {
                printf("cannot execute SHILL_POPULAR_PUSH, must call rankMovies first\n");
                return;
            }
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillNuke, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');

            list_t *topMovies = list_init();
            list_t *averageRank = list_init();
            list_t *numberOfRatings = list_init();
            for (int k = 0; k < self.shillNukePopularAmount + self.shillNukeRandomAmount; k++) {
                double maxAverageRank = -1;
                int ratingsCount;
                int topMovie;
                for (int j = 1; j < self.ranked -> length; j++) {
                    double compare = self.ranked -> data[j].r -> data[2].d * log(self.ranked -> data[j].r -> data[3].i + 1) / log(10);
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
            for (int j = 0; j < self.shillNukePopularAmount + self.shillNukeRandomAmount; j++) {
                int movieID = convertToInternalMovieID(selfp, self.movies -> data[0].r -> data[topMovies -> data[j].i].i);
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) movieID, 'i');
                if (self.ranked -> data[movieID].r -> data[2].d > self.globalAverageRating) {
                    list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 1.0, 'd');
                } else {
                    list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
                }
            }
            list_free(topMovies);
            list_free(averageRank);
            list_free(numberOfRatings);
        } else {
            printf("Error: shill strategy not recognised\n");
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
        /* MUST COPY TO AVOID DOUBLE FREE */
        list_t *copied = list_init();
        list_copy(copied, self.shills -> data[i].r);
        list_append(self.trainUsers, (unitype) copied, 'r');
    }
    *selfp = self;
}

/* calculates each movie's average rank. Also calculates global average rating
must call populateUsers prior to this function. This function only considers the ratings of trainUsers
*/
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
            self.globalAverageRating += self.trainUsers -> data[i].r -> data[2].r -> data[j].d;
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
    self.globalAverageRating /= (self.trainRatings -> data[0].r -> length - 1);
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

void runSingularTest(int predictionMethod, int numberOfShills, int shillStrategy) {
    model_t self;
    modelInit(&self);
    /* identify users */
    populateUsers(&self);
    generateUserReferenceCache(&self); // cached for speed
    generatePredictions(&self, PREDICTION_USER_USER);
    /* evaluate */
    printf("\nMAE Evaluation (No Shills): %lf\n", MAE(&self));
    printf("RMSE Evaluation (No Shills): %lf\n", RMSE(&self));
    if (shillStrategy > 10) {
        printf("Number of times \"Star Wars: Episode IV - A New Hope (1977)\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 1));
    } else {
        printf("Number of times \"Jumanji\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 0));
    }
    /* rank movies */
    rankMovies(&self);
    // printf("\nTop 10 Movies by ARL (No Shills):\n");
    // displayTopMovies(&self, self.ranked, 10);
    // printf("global average rating: %lf\n", self.globalAverageRating);
    /* generate shills */
    populateShills(&self, 100, shillStrategy);
    /* combine users and shills */
    combineUsersAndShills(&self);
    /* generate predictions */
    generateUserReferenceCache(&self); // cached for speed
    generatePredictions(&self, predictionMethod);
    /* evaluate */
    printf("\nMAE Evaluation (After Shills): %lf\n", MAE(&self));
    printf("RMSE Evaluation (After Shills): %lf\n", RMSE(&self));
    if (shillStrategy > 10) {
        printf("Number of times \"Star Wars: Episode IV - A New Hope (1977)\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 1));
    } else {
        printf("Number of times \"Jumanji\" appeared in top 10 recommendation list for test users (weighted so if it was number 1 it gets a score of 10): %d\n", SHE(&self, 10, 0));
    }
    /* rank movies */
    rankMovies(&self);
    printf("\nTop 10 Movies by ARL (After Shills):\n");
    displayTopMovies(&self, self.ranked, 10);
    model_free(&self, 0);
    // reset randomness
    // predSrand = 1;
}

int main(int argc, char  *argv[]) {
    int numberOfShills = 1;
    /* run push shills */
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_NAIVE_PUSH);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_RANDOM_PUSH);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_LOVE_HATE_PUSH);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_BANDWAGON_PUSH);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_POPULAR_PUSH);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_PROBE_PUSH);

    /* run nuke shills */
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_NAIVE_NUKE);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_RANDOM_NUKE);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_LOVE_HATE_NUKE);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_BANDWAGON_NUKE);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_POPULAR_NUKE);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_REVERSE_BANDWAGON_NUKE);
    runSingularTest(PREDICTION_USER_USER, numberOfShills, SHILL_PROBE_NUKE);
}