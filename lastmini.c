/*init 22.11.18*/
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <lime/LimeSuite.h>

const long int frq=100000;//100KHz
const double base=.2;//100KHz
const double   PI=3.14159265358979323846;
const double   PI2=3.14159265358979323846*2;
const double   PI_2=3.14159265358979323846/2;
const long int c=299792458;//     (m/sec)
/////////////////////////////////////////////////////////////////////
int main(int argc, char** argv){
  if ( argc < 2 ) {
    printf("--usage: %s <OPTIONS>\n", argv[0]);
    printf("  -f <FREQUENCY>\n"
           "  -b <BANDWIDTH_CALIBRATING> (default: 8e6)\n"
           "  -s <SAMPLE_RATE> (default: 5e6)\n"
           "  -g <GAIN_dB> range [0, 73] (default: 0)\n"
           "  -l <BUFFER_SIZE>  (default: 1000*1000)\n"
           "  -a <ANTENNA> (LNAL | LNAH | LNAW) (default: LNAW)\n"
           "  -o <OUTPUT_FILENAME CH A>\n"
           "  -p <OUTPUT_FILENAME CH B>\n"
           "  -n NUMBER of sampes\n"
           "  -m for m=1: mix@ stderr(I1,Q1,I2,Q2,I1,Q1,I2,Q2...)\n");
    return 1;
  }
  unsigned int freq = 2000000000;
  long long int maxnumber = 0;
  double bandwidth_calibrating = 2500000;
  double host_sample_rate = 5.0;
  double sample_rate = 5000000;
  double gain = 30;
  unsigned int buffer_size = 1000*1000;
  unsigned int device_i = 0;
  unsigned int channel = 0; // anything
  char* antenna = "LNAH";
  size_t anta = 2; //  1-RX LNA_H port   2-RX LNA_L port  3-RX LNA_W port
  size_t antb = 2; //  1-RX LNA_H port   2-RX LNA_L port  3-RX LNA_W port
  char* output_filename1 = "line-a.iq";
  char* output_filename2 = "line-b.iq";
  int nb_samples=0;
  int nb_samples2=0;
  int alg=0;
  for ( int i = 1; i < argc-1; i += 2 ) {
    if      (strcmp(argv[i], "-f") == 0) { freq = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-b") == 0) { bandwidth_calibrating = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-s") == 0) { sample_rate = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-g") == 0) { gain = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-l") == 0) { buffer_size = atoi( argv[i+1] ); }
    else if (strcmp(argv[i], "-a") == 0) { antenna = argv[i+1]; }
    else if (strcmp(argv[i], "-o") == 0) { output_filename1 = argv[i+1]; }
    else if (strcmp(argv[i], "-p") == 0) { output_filename2 = argv[i+1]; }
    else if (strcmp(argv[i], "-n") == 0) { maxnumber = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-m") == 0) { alg = atof( argv[i+1] ); }
  }
  lms_stream_meta_t meta;
  lms_device_t* device = NULL;
  struct s16iq_sample_s {  //definition bbufers
    short i;
    short q;
  } __attribute__((packed));
  struct s16iq_sample_s *buff1 = (struct s16iq_sample_s*)malloc(sizeof(struct s16iq_sample_s) * buffer_size);
  struct s16iq_sample_s *buff2 = (struct s16iq_sample_s*)malloc(sizeof(struct s16iq_sample_s) * buffer_size);
  struct s16iq_sample_s *mixbuf = (struct s16iq_sample_s*)malloc(sizeof(struct s16iq_sample_s) * buffer_size * 2);
  printf("\nfreq=%d\nsamplerate=%f\ngain=%f\nbufer size=%d\nalg=%d\n",freq,sample_rate,gain,buffer_size,alg);

  int device_count = LMS_GetDeviceList(NULL);
  lms_info_str_t device_list[ device_count ];
  LMS_GetDeviceList(device_list);
  LMS_Open(&device, device_list[ device_i ], NULL);

  LMS_Reset(device); //   reset
  LMS_Init(device);  //  default

  LMS_EnableChannel(device, LMS_CH_RX, 0, true);
  LMS_EnableChannel(device, LMS_CH_RX, 1, true);
  LMS_SetSampleRate(device, sample_rate, 0);
  LMS_GetSampleRate(device, LMS_CH_RX, channel, &host_sample_rate, NULL );
  LMS_SetAntenna( device, LMS_CH_RX, 0, anta );
  LMS_SetAntenna( device, LMS_CH_RX, 1, antb );
  LMS_Calibrate( device, LMS_CH_RX, 0, bandwidth_calibrating, 0 );
  LMS_Calibrate( device, LMS_CH_RX, 1, bandwidth_calibrating, 0 );
  LMS_SetLOFrequency( device, LMS_CH_RX, 0, freq);
  LMS_SetLOFrequency( device, LMS_CH_RX, 1, freq);
  LMS_SetGaindB( device, LMS_CH_RX, 0, gain );
  LMS_SetGaindB( device, LMS_CH_RX, 1, gain );

  lms_stream_t rx_stream = {
    .channel = 0,
    .fifoSize = buffer_size * sizeof(*buff1),
    .throughputVsLatency = 0.5,
    .isTx = LMS_CH_RX,
    .dataFmt = LMS_FMT_I16
  };
  lms_stream_t rx_stream2 = {
    .channel = 1,
    .fifoSize = buffer_size * sizeof(*buff2),
    .throughputVsLatency = 0.5,
    .isTx = LMS_CH_RX,
    .dataFmt = LMS_FMT_I16
  };
  LMS_SetupStream(device, &rx_stream);
  LMS_SetupStream(device, &rx_stream2);
  LMS_StartStream(&rx_stream);
  LMS_StartStream(&rx_stream2);

  FILE* fd1;
  FILE* fd2;
  FILE* fd = stderr;

  double defi;
  double gamma;
  double atg1;
  double atg2;
  double deltaphi(short i1,short q1,short i2,short q2){
    atg1=atan2(i1,q1);
    atg2=atan2(i2,q2);
    if (atg1>atg2) {return (atg1-atg2);} else {return(PI2+atg1-atg2);}
  }
  double azimut(long int frq, double deltaphase, double base){
//    double dl=c*deltaphase/(frq*PI2); // time=deltaphase/(frq*PI2); dl=c*time
    double azim=asin(c*deltaphase/(frq*PI2*base));  // azim=asin(dl/base);
    return azim;
  }
  if (alg!=1) {
    fd1 = fopen( output_filename1, "w+b" );
    fd2 = fopen( output_filename2, "w+b" );
    if ( fd1 == NULL || fd2 == NULL ) perror("fopen()");
  }
  int pointer = 0;
  while (maxnumber--!=0){ /////////////////// start main loop
    if (alg==0){
      nb_samples = LMS_RecvStream( &rx_stream, buff1, buffer_size, &meta, 100 );
      nb_samples2 = LMS_RecvStream( &rx_stream2, buff2, buffer_size, &meta, 100 );
      fwrite( buff1, sizeof( *buff1 ), nb_samples, fd1 );
      fwrite( buff2, sizeof( *buff2 ), nb_samples2, fd2 );
      fflush( fd2 );
      fflush( fd1 );
    }
    if (alg==1){
      nb_samples = LMS_RecvStream( &rx_stream, buff1, buffer_size, &meta, 100 );
      nb_samples2 = LMS_RecvStream( &rx_stream2, buff2, buffer_size, &meta, 100 );
      for (pointer=1;pointer<=buffer_size;pointer++){
        mixbuf[pointer*2]   = buff2[pointer];
        mixbuf[pointer*2-1] = buff1[pointer];
      }
      fwrite( mixbuf, sizeof( *mixbuf ), nb_samples+nb_samples2, fd );
      fflush(fd);
    }
    if (alg==2){
      nb_samples = LMS_RecvStream( &rx_stream, buff1, buffer_size, &meta, 100 );
      nb_samples2 = LMS_RecvStream( &rx_stream2, buff2, buffer_size, &meta, 100 );
      fwrite( buff1, sizeof( *buff1 ), nb_samples, fd1 );
      fwrite( buff2, sizeof( *buff2 ), nb_samples2, fd2 );
      fflush( fd2 );
      fflush( fd1 );
      for (pointer=1;pointer<=buffer_size;pointer++){
        if (buff1[pointer].q!=0 && buff2[pointer].q!=0){
          atg1=atan2(buff1[pointer].i,buff1[pointer].q);
          atg2=atan2(buff2[pointer].i,buff2[pointer].q);
        }
        if (atg1>atg2) {defi=atg1-atg2;} else {defi=PI2+atg1-atg2;}
        gamma=asin(c*defi/(frq*PI2*base))*180/PI; //gamma=(PI_2-acos(c*defi/(frq*PI2*base)))*180/PI;
        printf("%3.3f\n",gamma);
      }
    }
  }/////////////////// end main loop
  LMS_StopStream(&rx_stream);
  LMS_StopStream(&rx_stream2);
  LMS_DestroyStream(device, &rx_stream);
  LMS_DestroyStream(device, &rx_stream2);
  free( buff1 );
  free( buff2 );
  LMS_Close(device);
  if (alg==1) fclose( fd );
  if (alg==0){
    fclose( fd1 );
    fclose( fd2 );
    printf("Files :\n %s\n %s\nwas writed and closed.\n",output_filename1,output_filename2);
  }
  return 0;
}
