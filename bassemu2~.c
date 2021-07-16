 /* (c) 2006 Ch. Klippel
 * this software is gpl'ed software, read the file "LICENSE.txt" for details
 */

#include "m_pd.h"
#include "math.h"

// --------------------------------------------------------------------------- #
static t_class *be2_class;

#define PI   	   3.141592653589793
#define PI_2   	   6.283185307179586
#define SINFACT    12.56437
#define VCO_SAW    0
#define VCO_RECT   1
#define VCO_EXT    2
#define VCA_ATT    0
#define VCA_DEC    1
#define VCA_SIL    2
#define HPFREQ     50.0
#define FEGLPFREQ  300.0
#define SMFREQ     22.0


typedef struct _be2
{
  t_object x_obj;
  float vco_inc;
  float sig;
  float ideal_wave;
  int rst; /* reset osc */
  float hp_f;
  float hp_z;
  float vco_count;
  float pw;
  int vco_type;
  float pitch;
  float tune;
  float vcf_cutoff;
  float vcf_envmod;
  float vcf_envdecay;
  float decay;
  float vcf_reso;
  float vcf_rescoeff;
  float vcf_a; /* coef */
  float vcf_b;
  float vcf_c;
  float vcf_env; /* filter envelope */
  float vcf_d1; /* z */
  float vcf_d2;
  float vcf_e0; /* coef */
  float vcf_e1;
  float vcf_acor; /* res -> a cor */
  float vcf_eg_lp_z; /* feg lp */
  float vcf_eg_lp_f;
  float vca_attack; /* vca */
  float vca_decay;
  float vca_a;
  float vca_a0;
  int vca_mode;
  float sm_f; /* smooth filters */
  float cut_z;
  float res_z;
  float rcf_z;
  float env_z;
  float sr; /* sample rate */
} t_be2;


// --------------------------------------------------------------------------- #
inline void be2_recalc(t_be2 *x)
{
  x->vcf_e0 = exp(5.613 - 0.8000*(x->vcf_envmod) + 2.1553*(x->cut_z) 
                  - 0.7696*(1.0 - x->res_z));
  x->vcf_e0 *=PI/x->sr;

  x->vcf_e1 = exp(6.109 + 1.5876*(x->env_z) + 2.1553*(x->cut_z) 
                  - 1.2000*(1.0 - x->res_z));
  x->vcf_e1 *=PI/x->sr;
  x->vcf_e1 = x->vcf_e1 - x->vcf_e0;
  if (x->vcf_e1 > 1.0) x->vcf_e1 = 1.0; // clip

  x->vcf_acor = 1.0 - (x->res_z * 0.45); // comp res -> lvl
  if (x->vcf_acor < 0.0) x->vcf_acor = 0.0; // clip
}

// --------------------------------------------------------------------------- #
static void be2_calc_vco_inc(t_be2 *x)
{
  float p = x->pitch - 60.;
  p = p * 0.0833334;
  p = exp2(p);
  p = p * x->tune;
  p = (1. / x->sr) * p;
  if (p > 0.5) p = 0.5;
  x->vco_inc = p;
}


// --------------------------------------------------------------------------- #
static void be2_calc_decay(t_be2 *x)
{
  float f = x->decay * x->sr;
  x->vcf_envdecay = pow(0.1, (1.0 / f));
}

// --------------------------------------------------------------------------- #
static void be2_gate(t_be2 *x, t_floatarg f)
{
  if(f > 0)
    {
      x->vca_mode = VCA_ATT;
      x->vcf_env = x->vcf_e1;
      if (x->rst)
        {
          x->vcf_d1 = 0.0;
          x->vcf_d2 = 0.0;
          x->vco_count = 0.0;
        }
    }
  else
    {
      x->vca_mode = VCA_DEC;
    }
}

// --------------------------------------------------------------------------- #
static void be2_pitch(t_be2 *x, t_floatarg f)
{
  x->pitch = f;
  be2_calc_vco_inc(x);
}

// --------------------------------------------------------------------------- #
static void be2_vco(t_be2 *x, t_floatarg f)
{
  x->vco_type = f;
}

// --------------------------------------------------------------------------- #
static void be2_tune(t_be2 *x, t_floatarg f)
{
  x->tune = f;
  be2_calc_vco_inc(x);
}

// --------------------------------------------------------------------------- #
static void be2_cutoff(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vcf_cutoff = f;
}

// --------------------------------------------------------------------------- #
static void be2_reso(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  f = f*1.55;
  x->vcf_reso = f;
  x->vcf_rescoeff = exp(-1.20 + 3.455*(x->vcf_reso));
}

// --------------------------------------------------------------------------- #
static void be2_envmod(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->vcf_envmod = f;
}

// --------------------------------------------------------------------------- #
static void be2_decay(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  f = f*f*f;
  f = 0.044 + (2.2 * f);
  x->decay = f;
  be2_calc_decay(x);
}

// --------------------------------------------------------------------------- #
static void be2_pw(t_be2 *x, t_floatarg f)
{
  if      (f > 1.0) f = 1.0;
  else if (f < 0.0) f = 0.0;
  x->pw = f - 0.5;
}

// --------------------------------------------------------------------------- #
static void be2_rst(t_be2 *x, t_floatarg f)
{
  x->rst = f;
}

// --------------------------------------------------------------------------- #
static void be2_reset(t_be2 *x)
{
  x->vca_mode = VCA_SIL;
  x->vca_a = 0.0;
  x->vca_a0 = 0.5;
  x->vca_attack = 1.0 - ((1.0 / x->sr) / 0.0008); // sec
  x->vca_decay  = 1.0 - ((1.0 / x->sr) / 0.0043); // sec
  x->hp_f = (PI_2 / x->sr) * HPFREQ;
  x->hp_z = 0.0;
  x->vcf_eg_lp_f = (PI_2 / x->sr) * FEGLPFREQ;
  x->vcf_eg_lp_z = 0.0;
  x->sm_f = (PI_2 / x->sr) * SMFREQ;
  x->cut_z = 0.0;
  x->res_z = 0.0;
  x->rcf_z = 0.0;
  x->env_z = 0.0;
}

// --------------------------------------------------------------------------- #
static t_int *be2_perform(t_int *ww)
{
  t_be2 *x = (t_be2 *)(ww[1]);
  t_float *inbuf = (t_float *)(ww[2]);
  t_float *outbuf = (t_float *)(ww[3]);
  int n = (int)(ww[4]);
  
  float a;
  float w = 0;
  float k = 0;
  float ts;
  
  // only compute if needed .......
  if (x->vca_mode != VCA_SIL)
    {
      // begin be2 dsp engine
      while(n--)
        {
          // vco or ext in
          switch(x->vco_type)
            {
            case VCO_SAW :
              // count
              x->vco_count += x->vco_inc;
              if (x->vco_count > 0.5)
                x->vco_count = -0.5;
              
              // saw
              x->sig = (x->vco_count + 0.5) * 8.0;
              if (x->sig > 1.0) x->sig = 1.0;
              x->sig -= 0.5;
              x->sig *= 2.5;
              break;
              
            case VCO_RECT :
              // count
              x->vco_count += x->vco_inc;
              if (x->vco_count > 0.5)
                x->vco_count = -0.5;
              
              // pw
              if (x->vco_count <= x->pw)
                x->sig = -0.82;
              else
                x->sig = 0.8;
              break;
              
            case VCO_EXT :
              x->sig = (*inbuf++ * 0.48);
              // clip
              if (x->sig < -0.48) x->sig = -0.48;
              if (x->sig > 0.48)  x->sig = 0.48;
              break;
              
            default : 
              break;
            }
          
	  
          // hpf
          x->hp_z = (x->sig - x->hp_z) * x->hp_f + x->hp_z;
          ts = x->sig - x->hp_z;
          
          
          // update vca
          switch(x->vca_mode)
            {
            case VCA_ATT :
              x->vca_a += (x->vca_a0 - x->vca_a) * x->vca_attack;
              break;
              
            case VCA_DEC :
              x->vca_a *= x->vca_decay;
              if(x->vca_a < (0.00001))
                {
                  x->vca_a = 0;
                  x->vca_mode = VCA_SIL;
                }
              break;
              
            default : 
              break;
            }
          
          // feg and feg lp
          x->vcf_eg_lp_z = (x->vcf_env - x->vcf_eg_lp_z)
            * x->vcf_eg_lp_f + x->vcf_eg_lp_z;
          x->vcf_env *= x->vcf_envdecay;

          // smooth
          x->cut_z = (x->vcf_cutoff   - x->cut_z) * x->sm_f + x->cut_z;
          x->res_z = (x->vcf_reso     - x->res_z) * x->sm_f + x->res_z;
          x->rcf_z = (x->vcf_rescoeff - x->rcf_z) * x->sm_f + x->rcf_z;
          x->env_z = (x->vcf_envmod   - x->env_z) * x->sm_f + x->env_z;
          be2_recalc(x);

          // filter coef
          w = x->vcf_e0 + x->vcf_eg_lp_z;
          k = exp(-w/x->rcf_z);
          x->vcf_a = 2.0*cos(2.0*w) * k;
          x->vcf_b = -k*k;
          x->vcf_c = 1.0 - x->vcf_a - x->vcf_b;

          // compute sample
          ts = ts * x->vca_a;
          ts = ts * x->vcf_acor;

          // filter
          ts = x->vcf_a * x->vcf_d1
            +  x->vcf_b * x->vcf_d2
            +  x->vcf_c * ts;
          x->vcf_d2 = x->vcf_d1;
          x->vcf_d1 = ts;
          
          // limit (soft-never-clip)
          ts *= 0.333;
          a = ts;
          if (a < 0.) a = 0.-a; // abs
          a = a * 0.5;
          *(outbuf++) = ts / (a + 1.);
        }
    }
  else
    while(n--)
      {
        *outbuf++ = 0.0;
      }
  
  return (ww+5);
}

// --------------------------------------------------------------------------- #
static void be2_dsp(t_be2 *x, t_signal **sp)
{
  dsp_add(be2_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
  if (x->sr != sp[0]->s_sr)
    {
      x->sr = sp[0]->s_sr;
      be2_calc_vco_inc(x);
      be2_calc_decay(x);
      be2_reset(x);
    }
}

// --------------------------------------------------------------------------- #
static void *be2_new(void)
{
  t_be2 *x = (t_be2 *)pd_new(be2_class);
  outlet_new(&x->x_obj, gensym("signal"));
  x->sr = 44100.;
  be2_calc_vco_inc(x);
  be2_calc_decay(x);
  be2_reset(x);
  return (x);
}

// --------------------------------------------------------------------------- #
void bassemu2_tilde_setup(void)
{
  be2_class=class_new(gensym("bassemu2~"),
                      (t_newmethod)be2_new,0,sizeof(t_be2),0,A_GIMME,0);
  class_addmethod(be2_class, nullfn, gensym("signal"), 0);
  class_addmethod(be2_class,(t_method)be2_dsp,gensym("dsp"), 0);
  class_addmethod(be2_class,(t_method)be2_gate,gensym("gate"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_pitch,gensym("pitch"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_tune,gensym("tune"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_vco,gensym("vco"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_cutoff,gensym("cutoff"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_reso,gensym("reso"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_envmod,gensym("envmod"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_decay,gensym("decay"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_pw,gensym("pw"),A_DEFFLOAT, 0);
  class_addmethod(be2_class,(t_method)be2_rst,gensym("rst"),A_DEFFLOAT,0);
  class_addmethod(be2_class,(t_method)be2_reset,gensym("reset"),0);
}

