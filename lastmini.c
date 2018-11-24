/*init 22.11.18*/
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <lime/LimeSuite.h>
int limesdr_set_channel( const unsigned int freq,
  const double bandwidth_calibrating,
  const double gain,
  const unsigned int channel,
  const char* antenna,
  const int is_tx,
  lms_device_t* device )
{// anth1
  int nb_antenna = LMS_GetAntennaList(device, is_tx, channel, NULL);
  lms_name_t list[ nb_antenna ];
  LMS_GetAntennaList( device, is_tx, channel, list );
  int antenna_found = 0;
  int i;
  for ( i = 0; i < nb_antenna; i++ ) {
    if ( strcmp( list[i], antenna ) == 0 ) {
      antenna_found = 1;
      LMS_SetAntenna( device, is_tx, channel, i );			
    }
  }// anth2
  nb_antenna = LMS_GetAntennaList(device, is_tx, 1-channel, NULL);
  lms_name_t list2[ nb_antenna ];
  LMS_GetAntennaList( device, is_tx, 1-channel, list2 );
  antenna_found = 0;
  for ( i = 0; i < nb_antenna; i++ ) {
    if ( strcmp( list[i], antenna ) == 0 ) {
      antenna_found = 1;
      LMS_SetAntenna( device, is_tx, 1-channel, i );			
    }
  }//postinit
  LMS_Calibrate( device, is_tx, channel, bandwidth_calibrating, 0 );	 
  LMS_Calibrate( device, is_tx, 1-channel, bandwidth_calibrating, 0 );	 
  LMS_SetLOFrequency( device, is_tx, channel, freq);
  LMS_SetLOFrequency( device, is_tx, 1-channel, freq);
  LMS_SetGaindB( device, is_tx, channel, gain );
  LMS_SetGaindB( device, is_tx, 1-channel, gain );
  return 0;
}///// special init function
int limesdr_init( const double sample_rate,
       const unsigned int freq,
       const double bandwidth_calibrating,
       const double gain,
       const unsigned int device_i,
       const unsigned int channel,
       const char* antenna,
       const int is_tx,
       lms_device_t** device,
       double* host_sample_rate){
  int device_count = LMS_GetDeviceList(NULL);
  lms_info_str_t device_list[ device_count ];
  LMS_GetDeviceList(device_list);
  LMS_Open(device, device_list[ device_i ], NULL);
  LMS_Reset(*device);
  LMS_Init(*device);
  int is_not_tx = (is_tx == LMS_CH_TX) ? LMS_CH_RX : LMS_CH_TX;	
  LMS_EnableChannel(*device, LMS_CH_RX,   1-channel, true);
  LMS_EnableChannel(*device, LMS_CH_RX,   channel,   true);
  LMS_SetSampleRate( *device, sample_rate, 0 );
  LMS_GetSampleRate( *device, is_tx, channel, host_sample_rate, NULL );
  limesdr_set_channel( freq, bandwidth_calibrating, gain, channel, antenna, is_tx, *device );  
  return 0;
}/////////////////////////////////////////////////////////////////////
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
           "  -n NUMBER of sampes\n");
		return 1;
  }	
  unsigned int freq = 2000000000;
  long long int maxnumber = 0;
  double bandwidth_calibrating = 8e6;
  double host_sample_rate = 5.0;
  double sample_rate = 5000000;
  double gain = 30;
  unsigned int buffer_size = 1000*1000;
  unsigned int device_i = 0;
  unsigned int channel = 0; // anything  
  char* antenna = "LNAW";
  char* output_filename1 = "line-a.iq";  
  char* output_filename2 = "line-b.iq";  
  int nb_samples=0;
  int nb_samples2=0;
  lms_stream_meta_t meta;
  lms_device_t* device1 = NULL;
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
  }    
  struct s16iq_sample_s {  //definition bbufers
    short i;
    short q;
  } __attribute__((packed));
  struct s16iq_sample_s *buff1 = (struct s16iq_sample_s*)malloc(sizeof(struct s16iq_sample_s) * buffer_size);
  struct s16iq_sample_s *buff2 = (struct s16iq_sample_s*)malloc(sizeof(struct s16iq_sample_s) * buffer_size);  
  limesdr_init( sample_rate,freq,bandwidth_calibrating,gain,device_i,0,antenna,LMS_CH_RX,&device1,&host_sample_rate);
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
  LMS_SetupStream(device1, &rx_stream);
  LMS_SetupStream(device1, &rx_stream2);
  LMS_StartStream(&rx_stream);
  LMS_StartStream(&rx_stream2);
  FILE* fd1;
  FILE* fd2;
  fd1 = fopen( output_filename1, "w+b" );
  fd2 = fopen( output_filename2, "w+b" );
  if ( fd1 == NULL || fd2 == NULL ) perror("fopen()");
  while (maxnumber--!=0){ /////////////////// start main loop                 
    nb_samples = LMS_RecvStream( &rx_stream, buff1, buffer_size, &meta, 100 );
    nb_samples2 = LMS_RecvStream( &rx_stream2, buff2, buffer_size, &meta, 100 );
    fwrite( buff1, sizeof( *buff1 ), nb_samples, fd1 );
    fwrite( buff2, sizeof( *buff2 ), nb_samples2, fd2 );
    fflush( fd2 );
    fflush( fd1 );
  }/////////////////// end main loop
  LMS_StopStream(&rx_stream);
  LMS_StopStream(&rx_stream2);
  LMS_DestroyStream(device1, &rx_stream);
  LMS_DestroyStream(device1, &rx_stream2);
  free( buff1 );
  free( buff2 );
  printf("Files :\n %s\n %s\nis wited and closed.\n",output_filename1,output_filename2);
  LMS_Close(device1);
  fclose( fd1 );
  fclose( fd2 );
  return 0;
}
