/*
** ALEXNEX PROJECT, 2026
** sim/binding.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_BINDING_H
    #define SIM_BINDING_H

    #include <stdbool.h>
    #include <stddef.h>

/*
** Every key, named as in the settings file and listed in SFML 2.6's sfKeyCode
** order: input.c checks each one against sfKey<name> when it compiles, so the
** codes can be cast straight to sfKeyCode (FEATURES 2.2, PLAN 10.2).
*/
    #define KEY_LIST(m) \
    m(A) m(B) m(C) m(D) m(E) m(F) m(G) m(H) m(I) m(J) m(K) m(L) m(M) \
    m(N) m(O) m(P) m(Q) m(R) m(S) m(T) m(U) m(V) m(W) m(X) m(Y) m(Z) \
    m(Num0) m(Num1) m(Num2) m(Num3) m(Num4) m(Num5) m(Num6) m(Num7) \
    m(Num8) m(Num9) m(Escape) m(LControl) m(LShift) m(LAlt) m(LSystem) \
    m(RControl) m(RShift) m(RAlt) m(RSystem) m(Menu) m(LBracket) \
    m(RBracket) m(Semicolon) m(Comma) m(Period) m(Apostrophe) m(Slash) \
    m(Backslash) m(Grave) m(Equal) m(Hyphen) m(Space) m(Enter) \
    m(Backspace) m(Tab) m(PageUp) m(PageDown) m(End) m(Home) m(Insert) \
    m(Delete) m(Add) m(Subtract) m(Multiply) m(Divide) m(Left) m(Right) \
    m(Up) m(Down) m(Numpad0) m(Numpad1) m(Numpad2) m(Numpad3) m(Numpad4) \
    m(Numpad5) m(Numpad6) m(Numpad7) m(Numpad8) m(Numpad9) m(F1) m(F2) \
    m(F3) m(F4) m(F5) m(F6) m(F7) m(F8) m(F9) m(F10) m(F11) m(F12) \
    m(F13) m(F14) m(F15) m(Pause)

    /* "Mouse<name>", in sfMouseButton order. */
    #define MOUSE_LIST(m) m(Left) m(Right) m(Middle)

    #define JOY_BUTTONS 16            /* "Joy0" to "Joy15" */
    #define BINDING_NAME_MAX 16

    #define KEY_ENUM(name) KEY_##name,
    #define MOUSE_ENUM(name) MOUSE_##name,

typedef enum key_id { KEY_LIST(KEY_ENUM) KEY_COUNT } key_id_t;
typedef enum mouse_id { MOUSE_LIST(MOUSE_ENUM) MOUSE_COUNT } mouse_id_t;

typedef enum binding_kind {
    BIND_NONE,                        /* unbound                              */
    BIND_KEY,                         /* code: a key_id_t                     */
    BIND_MOUSE,                       /* code: a mouse_id_t                   */
    BIND_JOY                          /* code: a button of the first gamepad  */
} binding_kind_t;

typedef struct binding {
    binding_kind_t kind;
    int code;
} binding_t;

/* A name of the settings file, any case ("space" is Space). */
bool binding_parse(const char *name, binding_t *out);

/* The name the file gets: "Space", "MouseLeft", "Joy3"; "" when unbound. */
void binding_format(binding_t b, char *buf, size_t size);

bool binding_equal(binding_t a, binding_t b);

#endif
