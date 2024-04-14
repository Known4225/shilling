/*
Created by Ryan Srichai
https://github.com/Known4225
MIT licensed, this is open source software.

14.04.24:
A csv and tsv parser
This creates a list_t of list_t with the following format:
returned list:
[column1, column2, column3, column4, etc]
each column list:
[columnHeader (name), item1, item2, item3, etc]
To access item at row 5 column 2, do (assuming rows and columns are 0-indexed)
csvList -> data[2].r -> data[5]
And to get its type do
csvList -> data[2].r -> type[5];
*/
#include "list.h"

/* how long a line of the csv/tsv is allowed to be, an error will be printed and NULL returned if any line exceeds this value. Feel free to change to suit your needs */
#define MAX_LINE_SIZE 4096

/* the main parsing function */
list_t *parseAllTypes(char *filename, char *delimeters, char allStrings, char allowInts) {
    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL) {
        printf("parseAllTypes: could not open file %s\n", filename);
        return NULL;
    }
    /* generate buffers */
    char *lineBuffer = malloc(MAX_LINE_SIZE + 2);
    char *nextSegmentString = NULL;
    char *endptrDouble;
    char *endptrInt;
    char *fgetsReturn = NULL;
    list_t *newList = list_init();
    /* read file line by line */
    int lineNumber = 0;
    fgetsReturn = fgets(lineBuffer, MAX_LINE_SIZE + 2, fptr);
    if (fgetsReturn == NULL) {
        printf("parseAllTypes: could not read headers of %s\n", filename);
        return NULL;
    }
    while (fgetsReturn != NULL) {
        double nextSegmentDouble;
        int nextSegmentInt;
        nextSegmentString = strtok(lineBuffer, delimeters);
        int columnNumber = 0;
        while (nextSegmentString != NULL) {
            if (lineNumber == 0) {
                list_append(newList, (unitype) list_init(), 'r');
            }
            if (columnNumber >= newList -> length) {
                printf("parseAllTypes: row %d had too many columns\n", lineNumber);
                return NULL;
            }
            nextSegmentDouble = strtold(nextSegmentString, &endptrDouble);
            if (allStrings == 0 && allowInts) {
                nextSegmentInt = strtol(nextSegmentString, &endptrInt, 10);
            }
            if (allStrings || *endptrDouble != '\0' || lineNumber == 0) { // check if all strings flag is set or valid double or header (we always want headers to be string type)
                /* not a valid double, parse as string */
                // printf("strtod: not a valid double\n");
                list_append(newList -> data[columnNumber].r, (unitype) nextSegmentString, 's');
            } else {
                /* valid double, parse as double */
                // printf("strtod: %lf\n", parseDouble);,
                if (allowInts && *endptrInt == '\0') {
                    /* this is an int */
                    list_append(newList -> data[columnNumber].r, (unitype) nextSegmentInt, 'i');
                } else {
                    /* this is a double */
                    list_append(newList -> data[columnNumber].r, (unitype) nextSegmentDouble, 'd');
                }
            }
            nextSegmentString = strtok(NULL, delimeters);
            columnNumber++;
        }
        fgetsReturn = fgets(lineBuffer, MAX_LINE_SIZE + 2, fptr);
        lineNumber++;
    }
    fclose(fptr);
    return newList;
}

/* returns a list with the parsed CSV, all strings and headers are strings and all numbers are doubles */
list_t *parseCSV(char *filename) {
    return parseAllTypes(filename, ",\n", 0, 0);
}

/* returns a list with the parsed TSV, all strings and headers are strings and all numbers are doubles */
list_t *parseTSV(char *filename) {
    return parseAllTypes(filename, "\t\n", 0, 0);
}

/* parse CSV, but everything in the column lists is a string */
list_t *parseCSVAllString(char *filename) {
    return parseAllTypes(filename, ",\n", 1, 0);
}

/* parse TSV, but everything in the column lists is a string */
list_t *parseTSVAllString(char *filename) {
    return parseAllTypes(filename, "\t\n", 1, 0);
}

/* parse CSV, but all non-decimal numbers are ints */
list_t *parseCSVAllowInts(char *filename) {
    return parseAllTypes(filename, ",\n", 0, 1);
}

/* parse TSV, but all non-decimal numbers are ints */
list_t *parseTSVAllowInts(char *filename) {
    return parseAllTypes(filename, "\t\n", 0, 1);
}

/* the main packager function */
void packageAllTypes(list_t *csvList, char *filename, char CSVOrTSV, int truncateDoubles) {
    /* routine checks */
    if (csvList -> length == 0) {
        printf("packageAllTypes: list empty\n");
        return;
    }
    int sameLength = 0;
    if (csvList -> type[0] != 'r') {
        printf("packageAllTypes: list formatted incorrectly\n");
        return;
    }
    sameLength = csvList -> data[0].r -> length;
    for (int i = 1; i < csvList -> length; i++) {
        if (csvList -> type[i] != 'r') {
            printf("packageAllTypes: list formatted incorrectly\n");
            return;
        }
        if (csvList -> data[i].r -> length != sameLength) {
            printf("packageAllTypes: not all sublists are the same length\n");
            return;
        }
    }
    /* create buffers */
    FILE *fptr = fopen(filename, "w");
    char *lineBuffer = malloc(MAX_LINE_SIZE + 2);
    /* write rows */
    for (int lineNumber = 0; lineNumber < csvList -> data[0].r -> length; lineNumber++) {
        int lineBufferIndex = 0;
        for (int columnNumber = 0; columnNumber < csvList -> length - 1; columnNumber++) {
            /* write n - 1 columns */
            char *element = unitype_to_string_truncated_doubles(csvList -> data[columnNumber].r -> data[lineNumber], csvList -> data[columnNumber].r -> type[lineNumber], truncateDoubles); // maybe a little slow but certainly acceptable
            if (element == NULL) {
                printf("packageAllTypes: Error converting unitype to string (line %d column %d)\n", lineNumber, columnNumber);
                return;
            }
            if (lineBufferIndex > MAX_LINE_SIZE) {
                printf("packageAllTypes: line %d exceeds maximum line size\n");
            }
            strcpy(lineBuffer + lineBufferIndex, element);
            lineBufferIndex += strlen(element);
            if (CSVOrTSV) {
                /* tsv */
                strcpy(lineBuffer + lineBufferIndex, "\t");
            } else {
                /* csv */
                strcpy(lineBuffer + lineBufferIndex, ",");
            }
            lineBufferIndex += 1;
        }
        /* write last column, don't append tab/comma */
        char *element = unitype_to_string_truncated_doubles(csvList -> data[csvList -> length - 1].r -> data[lineNumber], csvList -> data[csvList -> length - 1].r -> type[lineNumber], truncateDoubles); // maybe a little slow but certainly acceptable
        if (element == NULL) {
            printf("packageAllTypes: Error converting unitype to string (line %d column %d)\n", lineNumber, csvList -> length - 1);
            return;
        }
        if (lineBufferIndex > MAX_LINE_SIZE) {
            printf("packageAllTypes: line %d exceeds maximum line size\n");
        }
        strcpy(lineBuffer + lineBufferIndex, element);
        fprintf(fptr, "%s\n", lineBuffer);
    }
    fclose(fptr);
}

/* takes a list and packages it to a CSV. Must be a list of lists */
void packageToCSV(list_t *csvList, char *filename, int truncate) {
    packageAllTypes(csvList, filename, 0, truncate);
}

void packageToTSV(list_t *csvList, char *filename, int truncate) {
    packageAllTypes(csvList, filename, 1, truncate);
}

void printRowCSV(list_t *csvList, int row) {
    if (csvList -> data[0].r -> length <= row) {
        printf("printRowCSV: no row %d exists\n", row);
        return;
    }
    int i = 0;
    for (; i < csvList -> length - 1; i++) {
        unitype_print(csvList -> data[i].r -> data[row], csvList -> data[i].r -> type[row]);
        printf(", ");
    }
    unitype_print(csvList -> data[i].r -> data[row], csvList -> data[i].r -> type[row]);
    printf("\n");
}