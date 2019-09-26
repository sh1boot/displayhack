#if !defined(WIN32SS_HOOK_H_INCLUDED)
#define WIN32SS_HOOK_H_INCLUDED

struct saver_priv_state_t;
typedef struct saver_priv_state_t saver_state_t;

typedef struct
{
   unsigned char b, g, r, a;
} saver_rgbquad;

typedef struct
{
   char const *name;
   int *dest;
   int def_value;
   int is_string;
} saver_config_item_t;

extern char const *SaverName;
extern int SaverTickPeriod;
extern const saver_config_item_t SaverConfig[];

saver_state_t *saver_init(int width, int height, int pitch);
void saver_keystroke(saver_state_t *state, int key);
int saver_advance(saver_state_t *state, void *buffer);
int saver_checkpalette(saver_state_t *state, saver_rgbquad const **palptr, int *start, int *count); /* return non-zero if changed */
void saver_shutdown(saver_state_t *state);

#endif /* !defined(WIN32SS_DRAW_H_INCLUDED) */
