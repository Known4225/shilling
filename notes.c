/*
Notes/Results:
So using User-user collaborative recommendation, After adding shills to the trainset the evaluation via RMSE and MAE actually got BETTER?
(as in, they are both lower after the shills were introduced). So.. wait was that expected? I thought it was supposed to be the other way around.
Here's some results in the console:

gcc shilling.c -O3 -o shilling.exe
shilling.exe
successfully loaded MovieLensData/movies.csv
successfully loaded MovieLensData/ratings.csv
initialisation complete!
Generated UserReferenceCache

MAE Evaluation (No Shills): 0.743219
RMSE Evaluation (No Shills): 0.970814

Top 10 Movies by ARL (No Shills):
1. (ID: 318, InternalID: 278) Shawshank Redemption, The (1994), score: 10.84 with 280 ratings
2. (ID: 356, InternalID: 315) Forrest Gump (1994), score: 10.25 with 289 ratings
3. (ID: 296, InternalID: 258) Pulp Fiction (1994), score: 10.20 with 262 ratings
4. (ID: 2571, InternalID: 1940) Matrix, The (1999), score: 10.12 with 251 ratings
5. (ID: 593, InternalID: 511) Silence of the Lambs, The (1991), score: 9.97 with 252 ratings
6. (ID: 260, InternalID: 225) Star Wars: Episode IV - A New Hope (1977), score: 9.92 with 222 ratings
7. (ID: 2959, InternalID: 2227) Fight Club (1999), score: 9.89 with 192 ratings
8. (ID: 527, InternalID: 462) Schindler's List (1993), score: 9.68 with 197 ratings
9. (ID: 50, InternalID: 47) Usual Suspects, The (1995), score: 9.58 with 181 ratings
10. (ID: 1196, InternalID: 899) Star Wars: Episode V - The Empire Strikes Back (1980), score: 9.56 with 181 ratings
Generated 140 shills with the SHILL_TARGETED_NUKE strategy
Generated UserReferenceCache

MAE Evaluation (After Shills): 0.732440
RMSE Evaluation (After Shills): 0.957760

Top 10 Movies by ARL (After Shills):
1. (ID: 1, InternalID: 1) Toy Story (1995), score: 8.23 with 472 ratings
2. (ID: 318, InternalID: 278) Shawshank Redemption, The (1994), score: 7.75 with 420 ratings
3. (ID: 6, InternalID: 6) Heat (1995), score: 7.68 with 84 ratings
4. (ID: 3793, InternalID: 2837) X-Men (2000), score: 7.67 with 121 ratings
5. (ID: 2918, InternalID: 2196) Ferris Bueller's Day Off (1986), score: 7.67 with 96 ratings
6. (ID: 1097, InternalID: 837) E.T. the Extra-Terrestrial (1982), score: 7.66 with 108 ratings
7. (ID: 3949, InternalID: 2946) Requiem for a Dream (2000), score: 7.65 with 82 ratings
8. (ID: 5418, InternalID: 3855) Bourne Identity, The (2002), score: 7.65 with 98 ratings
9. (ID: 1968, InternalID: 1446) Breakfast Club, The (1985), score: 7.65 with 100 ratings
10. (ID: 648, InternalID: 547) Mission: Impossible (1996), score: 7.64 with 148 ratings

The point is the MAE and RMSE evaluations before and after shills. It's a good 0.01 - 0.02 points lower after the shills. Is this a mistake on my part or is it intended?
This isn't an isolated case either, it's always a little bit lower after I introduce the shills, like without fail.

Okay but I suppose maybe these predictive metrics aren't really what we should be analysing. It's that recommendation list that should be "impacted greatly" by the shills since I'm doing user-user
Problem is, what do you mean "recommendation list". Like the recommendations are meant to be personalised. So should I make a recommendation list for each user?
the ARL metric generates a "common" recommendation list, but it's basically just a popularity contest with ARL. Here's what I'll do.

Count the number of times the shilled item appears on the top 10 of test users recommendation list (using user-user predictive scores to determine what a given user will like the most)
Then do the same after introducing the shills. And we'll compare how effective the shill attack is from that perspective.

Okay instead of just counting, scoring. So you get topX points if the shilled item is number 1 and 1 point if it's the last one

Okay so I found some interesting results. So when applying the SHİLL_TARGETED_NUKE strategy it actually has an opposite effect of REMOVING the shilled item from people's recommendation lists, but ARL
still says the shilled item is the most popular/well liked. This happens because targeted nuke shills rate the top 10 movies (according to ARL) a 0. This has the effect of making them very dissimilar to most
users and thus the shill attack does not work well. The SHILL_NAIVE_PUSH strategy works better here, but to get even better results the SHILL_BLEND_PUSH strategy makes the shills a lot more similar to other users

Actually the SHILL_NAIVE_PUSH doesn't seem to do much of anything ever with the user-user system
But the SHILL_BLEND_PUSH gets much higher scores on the SHE metric

AT LEAST THATS WHAT I WOULD SAY IF IT WERE TRUE
I don't even know whats going on, none of the shills have done what they were meant to do. If anything... the introduction of the shills makes the recommender BETTER and makes the shilled item do WORSE
for the testset...

The testset never includes shills btw, maybe that's my problem. Like obviously if there were shills in the testset then the SHE would be so much higher after shilling because the shills would all get recommended
the shilled item at the top of their list. But the whole point is that like we don't really care what the shills see on their recommendation list because they aren't real users. We only care about the
effect that they have on actual real users, which is what the testset is made up of. So I don't know why none of my shilling strategies have bore fruit.
Maybe I'm making too many shills? Let me try some other things

I'm nuking my own shillPush item now just to see what happens...
I'm being trolled.
I told my shills to nuke toy story and it got a higher score on the SHE metric after 10 shills rated the top 10 movies a 3/10 and... wait a second
I forgot to check if toy story is breaking the top 10... My shills might be doing something I dont want

No it's working fine, the shills are correctly shilling (i think)
It's just that my test users aren't seeing the devastating results of the shilling inflation ££
Thats so weird. I'll do another investigation tomorrow





Old Notes/Results (shill names and behaviours have since been altered):
with srand = time(NULL), 100 shills, shill fill and blend amounts are 10

Shill Strategy               Algorithm      SHE (no shills)     SHE (with 100 shills)    ShillPush    ShillNuke       Effect
SHILL_NAIVE_PUSH             user-user           122                    122                  1           NA           None
SHILL_RANDOM_FILL_PUSH       user-user           78                     105                  1           NA           Correct
SHILL_TARGETED_FILL_PUSH     user-user           111                    139                  1           NA           Correct
SHILL_RANDOM_BLEND_PUSH      user-user           132                    158                  1           NA           Correct
SHILL_TARGETED_BLEND_PUSH    user-user           89                     83                   1           NA           Opposite

SHILL_NAIVE_NUKE             user-user          1719                   1719                  NA          278          None
SHILL_RANDOM_FILL_NUKE       user-user          1601                   1951                  NA          278          Opposite
SHILL_TARGETED_FILL_NUKE     user-user          1866                   1652                  NA          278          Correct
SHILL_RANDOM_BLEND_NUKE      user-user          1781                   1717                  NA          278          Correct
SHILL_TARGETED_BLEND_NUKE    user-user          1723                   1625                  NA          278          Correct

I'm going to make one more table with srand set to a fixed value
with randPredictive (predSrand = 1), 100 shills, shill fill and blend amounts are 10

Shill Strategy               Algorithm      SHE (no shills)     SHE (with 100 shills)    ShillPush    ShillNuke       Effect
SHILL_NAIVE_PUSH             user-user           122                    122                  1           NA           None
SHILL_RANDOM_FILL_PUSH       user-user           122                    167                  1           NA           Correct
SHILL_TARGETED_FILL_PUSH     user-user           122                    142                  1           NA           Correct
SHILL_RANDOM_BLEND_PUSH      user-user           122                    167                  1           NA           Correct
SHILL_TARGETED_BLEND_PUSH    user-user           122                     85                  1           NA           Opposite

SHILL_NAIVE_NUKE             user-user           633                    633                  NA          278          None
SHILL_RANDOM_FILL_NUKE       user-user           633                    785                  NA          278          Opposite
SHILL_TARGETED_FILL_NUKE     user-user           633                    544                  NA          278          Correct
SHILL_RANDOM_BLEND_NUKE      user-user           633                    598                  NA          278          Correct
SHILL_TARGETED_BLEND_NUKE    user-user           633                    600                  NA          278          Correct

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

#include "include/list.h"

int main(int argc, char *argv[]) {
    /* quick testing binary search */
    list_t *test = list_init();
    for (int i = 0; i < 100; i++) {
        list_append(test, (unitype) i, 'i');
    }
    list_delete(test, 20);
    for (int i = 0; i < 100; i++) {
        printf("search for %d: %d\n", i, list_find_binary(test, i));
    }
}