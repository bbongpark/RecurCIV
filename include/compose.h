#pragma once
#include <stddef.h>
#include <stdbool.h>
#define COMPOSE_MAX_BYTES 512
#define COMPOSE_ROWS 6
#define COMPOSE_ROW_BYTES 129
typedef enum { COMPOSE_CANCELLED, COMPOSE_EMPTY, COMPOSE_TOO_LONG, COMPOSE_ACCEPTED } ComposeResult;
ComposeResult compose_validate(bool accepted,const char *text,size_t capacity,size_t *length);
size_t compose_page(const char *text,size_t length,size_t page,char rows[COMPOSE_ROWS][COMPOSE_ROW_BYTES]);
