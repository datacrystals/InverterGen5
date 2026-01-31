// Hardware configuration information

// Software configuration parameters
// Use care when changing, but generally safe
#define COMMUTATION_PATTERN_MAX_ZONES 12 // safe to change


// Default values, probably dont change
// but not quite as dangerous as limits
#define COMMUTATION_PATTERN_DEFAULT_HZ 2000 // default to this frequency if no matching zone is found


// LIMITS -- !!DANGER ZONE!! --
// probably dont change this unless you have a really good reason to
// might blow up things if you randomly change it

// Seriously, dont change this.

#define MAX_SWITCHING_FREQUENCY_HZ 10000
#define MIN_SWITCHING_FREQUENCY_HZ 600

#define MIN_FUNDAMENTAL_FREQUENCY_HZ -500
#define MAX_FUNDAMENTAL_FREQUENCY_HZ 500

#define MAX_FUNDAMENTAL_FREQUENCY_RAMP_HZS 500

// HARDWARE PIN CONFIGS (!!!DO NOT CHANGE THESE!!!)

// GPIO mapping
#define U_A 16
#define U_B 17
#define V_A 18
#define V_B 19
#define W_A 20
#define W_B 21