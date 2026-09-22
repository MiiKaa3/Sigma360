#ifndef FETCH_H
#define FETCH_H

int get_cookies();

int get_lecture(char* lecture);

int get_thumbnails(char* root, pid_t* pid);

int get_courses_json();

#endif // FETCH_H
