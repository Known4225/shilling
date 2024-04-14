/*
Created by Ryan Srichai
https://github.com/Known4225
MIT licensed, this is open source software.

I just think that ML in C needs to be a thing. I'm tired of waiting 40 minutes for my for loop to finish in python

13.04.24:
This project creates a movie recommender based on the MovieLens latest small database (as of 13.04.24)
It's a really bad recommender, here are the details:
 - it recommends the same movies to all users
 - it uses the average ranking of all users to determine what movies are recommended
 - It is evaluated on three metrics, RMSE, X, and S
*/
#include "include/csvParser.h"

/* shill strategy macros */
#define SHILL_NAIVE_PUSH 1
#define SHILL_NAIVE_NUKE 2

/*
Movies are given an internal ID that does not skip numbers, these are not the same as the movieID from the dataset
Users also have an ID that does not skip, but it is currently equal to the userID from the ratings list
If a user, such as user 0, has no movies rated then we just have them logged with no movies rated
*/

typedef struct {
    list_t *movies;
    list_t *ratings;
    list_t *internalMovieID;
    list_t *users; // spec: [userID (int), movies rated (list of ints (internalID)), ratings (list)]
    list_t *shills; // separate list of shills with the same spec as users
    list_t *ranked; // top ranked movies based on users list, spec: [movieIDInternal (int), movieName (string), averageRank (double)]

    int shillPush; // the ID of the movie that the shills are pushing
    int shillNukeRandom; // the number of random movies that a nuke shill will rate 0/5
} model_t;

/* random functions */
extern inline int randomInt(int lowerBound, int upperBound) { // random integer between lower and upper bound (inclusive)
    return (rand() % (upperBound - lowerBound + 1) + lowerBound);
}
extern inline double randomDouble(double lowerBound, double upperBound) { // random double between lower and upper bound
    return (rand() * (upperBound - lowerBound) / RAND_MAX + lowerBound); // probably works idk
}

void modelInit(model_t *selfp) {
    model_t self = *selfp;
    self.movies = parseTSVAllowInts("MovieLensData/movies.tsv");
    printf("successfully loaded MovieLensData/movies.csv\n");
    self.internalMovieID = list_init();
    for (int i = 0; i < self.movies -> data[0].r -> length; i++) {
        list_append(self.internalMovieID, self.movies -> data[0].r -> data[i], 'i');
    }
    self.ratings = parseCSVAllowInts("MovieLensData/ratings.csv");
    printf("successfully loaded MovieLensData/ratings.csv\n");
    /* print headers */
    printRowCSV(self.movies, 0);
    printRowCSV(self.ratings, 0);

    self.users = list_init();
    self.shills = list_init();
    self.ranked = list_init();

    self.shillPush = 2;
    self.shillNukeRandom = 10;
    *selfp = self;
}

int convertToInternalMovieID(model_t *selfp, int movieID) {
    return list_find(selfp -> internalMovieID, (unitype) movieID, 'i');
}

void populateUsers(model_t *selfp) {
    model_t self = *selfp;
    int userID = 0;
    int lineNumber = 0;
    /* loop through ratings */
    while (lineNumber < self.ratings -> data[0].r -> length) {
        list_append(self.users, (unitype) list_init(), 'r');
        list_append(self.users -> data[userID].r, (unitype) userID, 'i'); // userID
        list_append(self.users -> data[userID].r, (unitype) list_init(), 'r'); // list of movieIDs (that this shill has ranked)
        list_append(self.users -> data[userID].r, (unitype) list_init(), 'r'); // list of ratings
        if (userID != self.ratings -> data[0].r -> data[lineNumber].i) {
            lineNumber++;
        }
        while (lineNumber < self.ratings -> data[0].r -> length && userID == self.ratings -> data[0].r -> data[lineNumber].i) {
            /* add the movieID and rating to the user's rating lists */
            list_append(self.users -> data[userID].r -> data[1].r, (unitype) convertToInternalMovieID(selfp, self.ratings -> data[1].r -> data[lineNumber].i), 'i');
            list_append(self.users -> data[userID].r -> data[2].r, self.ratings -> data[2].r -> data[lineNumber], 'd');
            lineNumber++;
        }
        userID++;
        // printf("user: %d, lineNumber %d\n", userID, lineNumber);
    }
    // list_print(self.users -> data[606].r);
    *selfp = self;
}

/* create a bunch of shills */
void populateShills(model_t *selfp, int numberOfShills, int shillStrategy) {
    model_t self = *selfp;
    int shillID = self.users -> length; // start shill userIDs at the last userID + 1
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
            // the naive shill nuke rates the shilled movie a 5/5 and x other random movies a 0/5 (x is determined by self.shillNukeRandom)
            list_append(self.shills -> data[i].r -> data[1].r, (unitype) self.shillPush, 'i');
            list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 5.0, 'd');
            int randomMovie;
            for (int j = 0; j < self.shillNukeRandom; j++) {
                randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                while (list_count(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i') != -1) {
                    randomMovie = randomInt(0, self.movies -> data[0].r -> length - 1);
                }
                list_append(self.shills -> data[i].r -> data[1].r, (unitype) randomMovie, 'i');
                list_append(self.shills -> data[i].r -> data[2].r, (unitype) (double) 0.0, 'd');
            }
        }
    }
    *selfp = self;
}

/* put shills in user list */
void combineUsersAndShills(model_t *selfp) {
    model_t self = *selfp;
    for (int i = 0; i < self.shills -> length; i++) {
        list_append(self.users, (unitype) self.shills -> data[i].r, 'r');
    }
    *selfp = self;
}

void rankMovies(model_t *selfp) {
    model_t self = *selfp;
    list_clear(self.ranked);
    list_t *tempSumRanks = list_init();
    list_t *tempNumRatings = list_init();
    // printf("number of movies: %d\n", self.movies -> data[0].r -> length);
    for (int i = 0; i < self.movies -> data[0].r -> length; i++) {
        list_append(self.ranked, (unitype) list_init(), 'r');
        list_append(self.ranked -> data[i].r, self.movies -> data[0].r -> data[i], 'i'); // movieID
        list_append(self.ranked -> data[i].r, self.movies -> data[1].r -> data[i], 's'); // movieName
        list_append(self.ranked -> data[i].r, (unitype) (double) 0.0, 'd'); // average rank (unknown)

        list_append(tempSumRanks, (unitype) (double) 0.0, 'd');
        list_append(tempNumRatings, (unitype) 0, 'i');
    }
    for (int i = 0; i < self.users -> length; i++) {
        /* loop over all users */
        // printf("user: %d\n", self.users -> data[i].r -> data[0].i);
        for (int j = 0; j < self.users -> data[i].r -> data[1].r -> length; j++) {
            /* loop over all movies ranked by this user, add the rank to sumRanks and increment numRatings by 1 */
            // printf("user %d ranked movie %d a %lf\n", self.users -> data[i].r -> data[0].i, self.users -> data[i].r -> data[1].r -> data[j].i, self.users -> data[i].r -> data[2].r -> data[j].d);
            tempSumRanks -> data[self.users -> data[i].r -> data[1].r -> data[j].i].d += self.users -> data[i].r -> data[2].r -> data[j].d;
            tempNumRatings -> data[self.users -> data[i].r -> data[1].r -> data[j].i].i++;
        }
    }
    for (int i = 0; i < self.movies -> length; i++) {
        /* calculate average rank as sum of rankings / number of rankings */
        self.ranked -> data[i].r -> data[2].d = tempSumRanks -> data[i].d / tempNumRatings -> data[i].i;
    }
    list_free(tempSumRanks);
    list_free(tempNumRatings);
    *selfp = self;
}

void displayTopTenMovies(model_t *selfp, list_t *rankList) {
    list_t *topMovies = list_init();
    list_t *averageRank = list_init();
    for (int j = 0; j < 10; j++) {
        double maxAverageRank = -1;
        int topMovie;
        for (int i = 0; i < rankList -> length; i++) {
            if (list_count(topMovies, (unitype) i, 'i') == 0 && rankList -> data[i].r -> data[2].d > maxAverageRank) {
                maxAverageRank = rankList -> data[i].r -> data[2].d;
                topMovie = rankList -> data[i].r -> data[0].i;
            }
            
        }
        list_append(topMovies, (unitype) topMovie, 'i');
        list_append(averageRank, (unitype) maxAverageRank, 'd');
    }
    for (int i = 0; i < 10; i++) {
        printf("%d. (ID: %d) %s, average rating: %lf\n", i + 1, selfp -> movies -> data[0].r -> data[topMovies -> data[i].i].i, selfp -> movies -> data[1].r -> data[topMovies -> data[i].i].s, averageRank -> data[i].d);
    }
    list_free(topMovies);
    list_free(averageRank);
}

void RMSE() {

}

int main(int argc, char  *argv[]) {
    model_t self;
    modelInit(&self);
    /* identify users */
    populateUsers(&self);
    /* rank movies */
    rankMovies(&self);
    printf("Top 10 Movies by Average Ranking (No Shills):\n");
    displayTopTenMovies(&self, self.ranked);
    list_t *noShillRankings = list_init();
    list_copy(noShillRankings, self.ranked);
    /* generate shills */
    populateShills(&self, 50, SHILL_NAIVE_PUSH);
    /* combine users and shills */
    combineUsersAndShills(&self);
    /* rank movies */
    rankMovies(&self);
    printf("Top 10 Movies by Average Ranking (After Shills):\n");
    displayTopTenMovies(&self, self.ranked);
    
}