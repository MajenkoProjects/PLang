#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <ncurses.h>

const uint32_t    L           = 0x0000;
const uint32_t    V           = 0x0001;
const uint32_t    LL          = 0x0000;
const uint32_t    LV          = 0x0001;
const uint32_t    VL          = 0x0002;
const uint32_t    VV          = 0x0003;
const uint32_t    NOP         = 0x0000;
const uint32_t    MODE        = 0x0010;
const uint32_t    CALL        = 0x0020;
const uint32_t    GOTO        = 0x0030;
const uint32_t    RETURN      = 0x0040;
const uint32_t    SET         = 0x0050;
const uint32_t    DISPLAY     = 0x0060;
const uint32_t    DELAY       = 0x0070;
const uint32_t    DEC         = 0x0080;
const uint32_t    INC         = 0x0090;
const uint32_t    IF          = 0x00A0;
const uint32_t    PLAY        = 0x00B0;

const uint32_t    FALLING     = 0;
const uint32_t    RISING      = 1;
const uint32_t    CHANGE      = 2;

typedef enum {
    READS,
    EQ,
    GE,
    GT,
    LE,
    LT
} OPERATOR;

struct variable;

struct op {
    char *label;
    uint32_t opcode;
    struct op *next;
    struct op *alternate;
    uint32_t ival1;
    uint32_t ival2;
    uint32_t ival3;
    char *cval1;
    char *cval2;
    char *cval3;
    struct variable *vval1;
    struct variable *vval2;
    struct variable *vval3;
    uint32_t line;
};

typedef struct op op;

struct event {
    uint32_t type;
    uint32_t source;
    char *label;
    op *entry;
    op *current;
    uint32_t last;
    struct event *next;
};

typedef struct event event;

struct variable {
    char *name;
    uint32_t value;
    struct variable *next;
};

typedef struct variable variable;

struct stack {
    op *opcode;
    struct stack *next;
};

// Stub!
int ins[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

int digitalRead(int pin) {
    if (pin < 10) {
        return ins[pin];
    }

    return 0;
}

void pinMode(int pin, int mode) {
}

struct timeval bootTime;

uint32_t millis() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    uint64_t then = bootTime.tv_sec * 1000 + bootTime.tv_usec / 1000;
    uint64_t now = currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000;
    now = now - then;

    return now;
}


typedef struct stack stack;

op *program = NULL;
variable *variables = NULL;
event *events = NULL;

stack *callstack = NULL;

void plang_init() {
}

void syntaxerror(const char *c, uint32_t lineno) {
    printf("%s at line %d\n", c, lineno);
}

static inline bool isNumber(const char *c) {
    return (c[0] >= '0' && c[0] <= '9');
}

void push(op *oc) {
    stack *s = (stack *)malloc(sizeof(stack));
    s->opcode = oc;
    s->next = NULL;
    if (callstack == NULL) {
        callstack = s;
    } else {
        stack *scan = callstack;
        while (scan->next) {
            scan = scan->next;
        }
        scan->next = s;
    }
}

op *pop() {
    if (callstack == NULL) {
        return NULL;
    }

    if (callstack->next == NULL) {
        op *opcode = callstack->opcode;
        free(callstack);
        callstack = NULL;
        return opcode;
    }

    stack *scan = callstack;;
    stack *last = NULL;
    while (scan->next) {
        last = scan;
        scan = scan->next;
    }
    op *opcode = scan->opcode;
    free(scan);
    if (last = NULL) {
        last->next = NULL;
    }
    return opcode;
}

void addEvent(uint32_t type, uint32_t source, const char *label) {
    event *e = (event *)malloc(sizeof(event));
    e->source = source;
    e->type = type;
    e->label = strdup(label);
    e->next = NULL;
    e->current = NULL;
    e->last = digitalRead(e->source);
    if (events == NULL) {
        events = e;
    } else {
        event *scan = events;
        while (scan->next) {
            scan = scan->next;
        }
        scan->next = e;
    }
}

void addOpcode(op *oc) {
    if (program == NULL) {
        program = oc;
    } else {
        op *scan = program;
        while (scan->next) {
            scan = scan->next;
        }
        scan->next = oc;
        // If this happens to have an alternate route set then make that alternate have the
        // next command point to this one as well.  That way IF commands will continue properly.
        // Goto and Call are handled specially elsewhere.
        if (scan->alternate != NULL) {
            scan->alternate->next = oc;
        }
    }
}

variable *findVariable(char *name) {
    variable *scan = variables;
    while (scan) {
        if (!strcasecmp(scan->name, name)) {
            return scan;
        }
        scan = scan->next;
    }
    return NULL;
}

void freeop(op *c) {
    if (c->label != NULL) free(c->label);
    if (c->cval1 != NULL) free(c->cval1);
    if (c->cval2 != NULL) free(c->cval2);
    if (c->cval3 != NULL) free(c->cval3);
    if (c->alternate != NULL) freeop(c->alternate);
    free(c);
}

op *createOpcode(char *label, char *code, char *params, uint32_t line) {
    op *newop = (op *)malloc(sizeof(op));
    newop->opcode = NOP;
    newop->next = NULL;
    newop->alternate = NULL;
    newop->ival1 = 0;
    newop->ival2 = 0;
    newop->ival3 = 0;
    newop->cval1 = NULL;
    newop->cval2 = NULL;
    newop->cval3 = NULL;
    newop->vval1 = NULL;
    newop->vval2 = NULL;
    newop->vval3 = NULL;
    if (label != NULL) {
        newop->label = strdup(label);
    } else {
        newop->label = NULL;
    }
    newop->line = line;

    if (!strcasecmp(code, "NOP")) {
        return newop;
    }

    if (!strcasecmp(code, "MODE")) {
        char *pin = strtok(params, " \t");
        char *mode = strtok(NULL, " \t");
        char *extra = strtok(NULL, " \t");

        newop->opcode = MODE;

        if (pin == NULL || mode == NULL) {
            syntaxerror("Bad pin mode", line);
            freeop(newop);
            return NULL;
        }

        // Is it a number?
        if (isNumber(pin)) {
            newop->opcode |= L;
            newop->ival1 = atoi(pin);
        } else {
            newop->opcode |= V;
            newop->vval1 = findVariable(pin);
            if (newop->vval1 == NULL) {
                syntaxerror("No such variable", line);
                freeop(newop);
                return NULL;
            }
        }

        if (!strcasecmp(mode, "IN")) {
            newop->ival2 = 1;
        } else if (!strcasecmp(mode, "OUT")) {
            newop->ival2 = 0;
        } else {
            syntaxerror("Bad mode", line);
            freeop(newop);
            return NULL;
        }
        if (extra != NULL) {
            if (!strcasecmp(extra, "PULLUP")) {
                newop->ival3 = 1;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "IF")) {
        char *left = strtok(params, " \t");
        char *oper = strtok(NULL, " \t");
        char *right = strtok(NULL, " \t");
        char *altop = strtok(NULL, " \t");
        char *altparm = strtok(NULL, "\0");

        newop->opcode = IF;

        if (altop == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }

        if (isNumber(left) && isNumber(right)) {
            newop->ival1 = atoi(left);
            newop->ival3 = atoi(right);
            newop->opcode |= LL;
        } else if (isNumber(left) && !isNumber(right)) {
            newop->ival1 = atoi(left);
            newop->vval3 = findVariable(right);
            if (newop->vval3 == NULL) {
                syntaxerror("Unknown variable", line);
                freeop(newop);
                return NULL;
            }
            newop->opcode |= LV;
        } else if (!isNumber(left) && !isNumber(right)) {
            newop->vval1 = findVariable(left);
            if (newop->vval1 == NULL) {
                syntaxerror("Unknown variable", line);
                freeop(newop);
                return NULL;
            }
            newop->vval3 = findVariable(right);
            if (newop->vval3 == NULL) {
                syntaxerror("Unknown variable", line);
                freeop(newop);
                return NULL;
            }
            newop->opcode |= VV;
        } else if (!isNumber(left) && isNumber(right)) {
            newop->vval1 = findVariable(left);
            if (newop->vval1 == NULL) {
                syntaxerror("Unknown variable", line);
                freeop(newop);
                return NULL;
            }
            newop->ival3 = atoi(right);
            newop->opcode |= VL;
        } else {
            syntaxerror("Bad operands to if", line);
            freeop(newop);
            return NULL;
        }


        // Operators
        if (!strcasecmp(oper, "GE")) newop->ival2 = (uint32_t)GE;
        else if (!strcasecmp(oper, "GT")) newop->ival2 = (uint32_t)GT;
        else if (!strcasecmp(oper, "LE")) newop->ival2 = (uint32_t)LE;
        else if (!strcasecmp(oper, "LT")) newop->ival2 = (uint32_t)LT;
        else if (!strcasecmp(oper, "EQ")) newop->ival2 = (uint32_t)EQ;
        else if (!strcasecmp(oper, "READS")) newop->ival2 = (uint32_t)READS;
        else {
            syntaxerror("Bad operator", line);
            freeop(newop);
            return NULL;
        }


            



        op *altcmd = createOpcode(NULL, altop, altparm, line);
        if (altcmd == NULL) {
            freeop(newop);
            return NULL;
        }
        newop->alternate = altcmd;
        return newop;
    }

    if (!strcasecmp(code, "GOTO")) {
        if (params == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }
        // We can't know where the label is yet, so just store the name. We'll convert in the
        // second pass.
        newop->cval1 = strdup(params);
        newop->opcode = GOTO;
        return newop;
    }

    if (!strcasecmp(code, "PLAY")) {
        if (params == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }
        newop->cval1 = strdup(params);
        newop->opcode = PLAY;
        return newop;
    }

    if (!strcasecmp(code, "SET")) {
        char *dest = strtok(params, " \t");
        char *src = strtok(NULL, " \t");
        newop->opcode = SET;
        if (src == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }
        newop->vval1 = findVariable(dest);
        if (newop->vval1 == NULL) {
            syntaxerror("Unknown variable", line);
            freeop(newop);
            return NULL;
        }
        if (isNumber(src)) {
            newop->ival2 = atoi(src);
            newop->opcode |= L;
        } else {
            newop->vval2 = findVariable(src);
            newop->opcode |= V;
            if (newop->vval2 == NULL) {
                syntaxerror("Unknown variable", line);
                freeop(newop);
                return NULL;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "DISPLAY")) {
        newop->opcode = DISPLAY;
        if (params == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }
        if (isNumber(params)) {
            newop->ival1 = atoi(params);
            newop->opcode |= L;
        } else {
            newop->vval1 = findVariable(params);
            newop->opcode |= V;
            if(newop->vval1 == NULL) {
                syntaxerror("Unknown variable", line);
                freeop(newop);
                return NULL;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "DELAY")) {
        newop->opcode = DELAY;
        if (params == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }
        if (isNumber(params)) {
            newop->ival1 = atoi(params);
            newop->opcode |= L;
        } else {
            newop->vval1 = findVariable(params);
            newop->opcode |= V;
            if(newop->vval1 == NULL) {
                syntaxerror("Unknown variable", line);
                freeop(newop);
                return NULL;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "DEC")) {
        if (params == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }
        newop->vval1 = findVariable(params);
        newop->opcode = DEC;
        if(newop->vval1 == NULL) {
            syntaxerror("Unknown variable", line);
            freeop(newop);
            return NULL;
        }
        return newop;
    }

    if (!strcasecmp(code, "INC")) {
        if (params == NULL) {
            syntaxerror("Syntax error", line);
            freeop(newop);
            return NULL;
        }
        newop->vval1 = findVariable(params);
        newop->opcode = INC;
        if(newop->vval1 == NULL) {
            syntaxerror("Unknown variable", line);
            freeop(newop);
            return NULL;
        }
        return newop;
    }

    if (!strcasecmp(code, "RETURN")) {
        newop->opcode = RETURN;
        return newop;
    }

    syntaxerror("Unknown command", line);

    return NULL;
}

op *findLabel(const char *label) {
    op *scan = program;
    while (scan) {
        if (scan->label != NULL) {
            if (!strcasecmp(scan->label, label)) {
                return scan;
            }
        }
        scan = scan->next;
    }
    return NULL;
}

// Scan through looking for labels. Update the alternate pointer to the
// destination of the label.
bool plang_pass2() {
    op *scan = program;
    while (scan) {
        if ((scan->opcode & 0xFFF0) == GOTO) {
            op *lab = findLabel(scan->cval1);
            if (lab == NULL) {
                syntaxerror("Unknown label", scan->line);
                return NULL;
            }
            scan->alternate = lab;
            free(scan->cval1);
        } else if ((scan->opcode & 0xFFF0) == IF) {
            if (scan->alternate == NULL) {
                syntaxerror("Invalid jump location", scan->line);
            } else {
                if (scan->alternate->opcode == GOTO) {
                    op *lab = findLabel(scan->alternate->cval1);
                    if (lab == NULL) {
                        syntaxerror("Unknown label", scan->line);
                        return NULL;
                    }
                    scan->alternate->alternate = lab;
                    free(scan->alternate->cval1);
                }
            }
        }
        scan = scan->next;
    }

    event *escan = events;
    while (escan) {
        escan->entry = findLabel(escan->label);
printf("Adding link to %s\n", escan->label);
if (escan->entry == NULL) {
    printf("Failed\n");
}        
        //free(escan->label);
        escan = escan->next;
    }
}

bool plang_parse(char *line, uint32_t lineno) {
    char *label = NULL;
    char *opcode = NULL;

    while (strlen(line) > 0 && line[strlen(line)-1] < ' ') {
        line[strlen(line)-1] = 0;
    }
    if (strlen(line) == 0) {
        return true;
    }

    opcode = strtok(line, " \t");
    if (opcode[0] == '#') {
        return true;
    }
    if (opcode[strlen(opcode)-1] == ':') {
        opcode[strlen(opcode)-1] = 0;
        label = opcode;
        opcode = strtok(NULL, " \t");
    }

    // First the def directive. This isn't really a program instruction
    // so we don't want to create an opcode for it.

    if (!strcasecmp(opcode, "DEF")) {
        variable *var = (variable *)malloc(sizeof(variable));
        char *vname = strtok(NULL, " \t");
        if (vname == NULL) {
            syntaxerror("Syntax error", lineno);
            return false;
        }
        char *defval = strtok(NULL, " \t");
        int dv;
        if (defval != NULL) {
            dv = atoi(defval);
        } else {
            dv = 0;
        }
    
        var->name = strdup(vname);
        var->value = dv;
        var->next = NULL;
        if (variables == NULL) {
            variables = var;
        } else {
            variable *scan = variables;
            while (scan->next) {
                scan = scan->next;
            }
            scan->next = var;
        }
        return true;        
    }

    // Also the LINK command isn't a real command but an instruction
    // to the language to link a specific function to an event.

    if (!strcasecmp(opcode, "LINK")) {
        char *pin = strtok(NULL, " \t");
        char *type = strtok(NULL, " \t");
        char *label = strtok(NULL, " \t");
        uint32_t ntype = 0;
        uint32_t npin = 0;

        if (!strcasecmp(type, "RISING")) {
            ntype = RISING;
        } else if (!strcasecmp(type, "FALLING")) {
            ntype = FALLING;
        } else if (!strcasecmp(type, "CHANGE")) {
            ntype = CHANGE;
        } else {
            syntaxerror("Bad event type", lineno);
            return false;
        }

        if (isNumber(pin)) {
            npin = atoi(pin);
        } else {
            variable *v = findVariable(pin);
            if (v == NULL) {
                syntaxerror("Unknown variable", lineno);
                return false;
            }
            npin = v->value;
        }

        addEvent(ntype, npin, label);
        return true;
    }

    op *newop = createOpcode(label, opcode, strtok(NULL, "\0"), lineno);
    if (newop == NULL) {
        return false;
    }
    addOpcode(newop);
    return true;
}

void plang_exec(op **where) {
    char temp[1000];
    int vars = (*where)->opcode & 0x000F;
    uint32_t left;
    uint32_t right;
    int mode = 0;

    // Are we in a delay loop at the moment?
    if (((*where)->opcode & 0xFFF0) == DELAY) {
        if ((*where)->ival2 > 0) { // This is a delay we have started
            uint32_t dellen = vars & V ? (*where)->vval1->value : (*where)->ival1;
            // Still delaying?
            if ((millis() - (*where)->ival2) < dellen) {
                return;
            } else {
                // Delay finished - move on to the next op-code.
                (*where)->ival2 = 0; // Cancel the delay
                *where = (*where)->next;
                return;
            }
        }
    }

    bool result = false;
    switch ((*where)->opcode & 0xFFF0) {
        case NOP:
            break;
        case PLAY:
            sprintf(temp, "aplay -q %s &", (*where)->cval1);
            system(temp);
            break;
        case RETURN:
            *where = pop();
            return;
        case MODE:
            mode = (*where)->ival2;
            if (mode == 1 && (*where)->ival3 == 1) {
                mode = 2; // INPUT_PULLUP
            }
            pinMode(vars & V ? (*where)->vval1->value : (*where)->ival1, mode);
            break;
        case DISPLAY:
            mvprintw(0, 0, "Display: %04d\n",
                vars == L ? (*where)->ival1 : (*where)->vval1->value);
            break;
        case IF:
            // Select the operator
            left = vars & VL ? (*where)->vval1->value : (*where)->ival1;
            right = vars & LV ? (*where)->vval3->value : (*where)->ival3;
            switch ((*where)->ival2) {
                case EQ: result = (left == right); break;
                case GE: result = (left >= right); break;
                case GT: result = (left > right); break;
                case LE: result = (left <= right); break;
                case LT: result = (left < right); break;
                case READS: result = (digitalRead(left) == right); break;
                default:
                    syntaxerror("Bad operator", (*where)->line);
                    break;
            }
            if (result) {
                *where = (*where)->alternate;
                return;
            }
            break;
        case SET:
            (*where)->vval1->value = (vars & V) ? (*where)->vval2->value : (*where)->ival2;
            break;
        case GOTO:
            *where = (*where)->alternate;
            return;
        case DEC:
            if ((*where)->vval1->value > 0) {
                (*where)->vval1->value--;
            }
            break;
        case INC:
            (*where)->vval1->value++;
            break;
        case DELAY:
            (*where)->ival2 = millis();
            return;
        default: break;
    }
    *where = (*where)->next;
}

void updateIO() {
    mvprintw(1, 0, "Inputs: ");
    for (int i = 0; i < 10; i++) {
        mvprintw(1, 10 + i, "%d", ins[i]);
    }
}

void plang_run() {
    op *init = findLabel("init");
    while (init) {
        plang_exec(&init);
    }
    while (1) {
        int c = getch();
        switch (c) {
            case '0': ins[0] = 1 - ins[0]; break;
            case '1': ins[1] = 1 - ins[1]; break;
            case '2': ins[2] = 1 - ins[2]; break;
            case '3': ins[3] = 1 - ins[3]; break;
            case '4': ins[4] = 1 - ins[4]; break;
            case '5': ins[5] = 1 - ins[5]; break;
            case '6': ins[6] = 1 - ins[6]; break;
            case '7': ins[7] = 1 - ins[7]; break;
            case '8': ins[8] = 1 - ins[8]; break;
            case '9': ins[9] = 1 - ins[9]; break;
            case 'q': return;
        }
        updateIO();
        event *scan = events;
        while (scan) {
            if (scan->current != NULL) {
mvprintw(10, 10, "Exec: %s                    ", scan->label);
                plang_exec(&(scan->current));
            } else {
                uint32_t n = digitalRead(scan->source);
                if (n != scan->last) {
                    scan->last = n;
                    if (n == 0 && ((scan->type == FALLING) || (scan->type == CHANGE))) {
                        scan->current = scan->entry;
printw("Starting %s\n", scan->label);
                    } else if (n == 1 && ((scan->type == RISING) || (scan->type == CHANGE))) {
printw("Starting %s\n", scan->label);
                        scan->current = scan->entry;
                    }
                }
            }
            scan = scan->next;
        }
    }
}

void cleanexit() {
    endwin();
    return;
}


int main(int argc, char **argv) {

    atexit(cleanexit);

    gettimeofday(&bootTime, NULL);


    FILE *f;
    char temp[1024];
    uint32_t lineno = 1;

    
    if (argc != 2) {
        printf("Usage: plang <script>\n");
        return 10;
    }

    f = fopen(argv[1], "r");
    if (!f) {
        printf("Unable to open %s\n", argv[1]);
        return 10;
    }

    while (fgets(temp, 1023, f) > 0) {
        if (!plang_parse(temp, lineno)) {
            fclose(f);
            return 10;
        }
        lineno++;
    }

    fclose(f);

    plang_pass2();

    initscr();
    raw();
    cbreak();
    noecho();
    timeout(0);

    plang_run();
    endwin();

    return 0;
}


