#ifndef PIPELINE_H
#define PIPELINE_H

// Takes the full array of arguments (e.g., ["ls", "-l", "|", "wc", "-l", NULL])
// and executes the pipeline.
void execute_pipeline(char **argv);

#endif
