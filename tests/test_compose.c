#include "compose.h"
#include "text.h"
#include "typer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    size_t n=99;char text[COMPOSE_MAX_BYTES+2];
    assert(compose_validate(false,"draft",6,&n)==COMPOSE_CANCELLED && n==0);
    assert(compose_validate(true,"",1,&n)==COMPOSE_EMPTY);
    memset(text,'a',COMPOSE_MAX_BYTES);text[COMPOSE_MAX_BYTES]=0;
    assert(compose_validate(true,text,sizeof(text),&n)==COMPOSE_ACCEPTED && n==COMPOSE_MAX_BYTES);
    text[COMPOSE_MAX_BYTES]='a';text[COMPOSE_MAX_BYTES+1]=0;
    assert(compose_validate(true,text,sizeof(text),&n)==COMPOSE_TOO_LONG);
    assert(compose_validate(true,text,COMPOSE_MAX_BYTES+1,&n)==COMPOSE_TOO_LONG);
    char rows[COMPOSE_ROWS][COMPOSE_ROW_BYTES];
    const char *lines="one\ntwo\nthree\nfour\nfive\nsix\nseven";
    assert(compose_page(lines,strlen(lines),0,rows)==2);
    assert(!strcmp(rows[0],"one") && !strcmp(rows[5],"six"));
    assert(compose_page(lines,strlen(lines),1,rows)==2 && !strcmp(rows[0],"seven"));
    const char *utf="1234567890123456789012345678901\xc3\xa9Z";
    assert(compose_page(utf,strlen(utf),0,rows)==1);
    assert(!strcmp(rows[1],"Z") && strlen(rows[0])==33);
    {
        TextPlan plan;TextError error;
        const char *draft="A !?\nB";
        assert(text_plan(draft,strlen(draft),false,&plan,&error));
        assert(plan.count==6 && plan.strokes[0].modifier==2 && plan.strokes[4].key==0x28);
        Typer t;Stroke report;typer_start(&t,plan.strokes,plan.count,0,0);
        assert(typer_step(&t,0,false,&report) && report.key==0);
        assert(typer_step(&t,1,false,&report) && report.key==plan.strokes[0].key && report.modifier==2);
        assert(typer_step(&t,2,false,&report) && report.key==0 && report.modifier==0);
        text_plan_free(&plan);
    }
    puts("PASS: compose validation, cancellation, pagination, UTF-8, layouts, punctuation, and typer reports.");
    return 0;
}
