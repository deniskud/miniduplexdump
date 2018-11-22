/*22.11.18*/
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <lime/LimeSuite.h>
//// special postinit function :
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
       double* host_sample_rate
){
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
int main(){
  unsigned int freq = 2000000000;
  double bandwidth_calibrating = 8e6;
  double host_sample_rate = 5.0;
  double sample_rate = 5000000;
  double gain = 30;
  unsigned int buffer_size = 1000*1000;
  unsigned int device_i = 0;
  unsigned int channel = 0; // anything  
  char* antenna = "LNAW";
  int nb_samples=0;
  int nb_samples2=0;
  lms_stream_meta_t meta;
  lms_device_t* device1 = NULL;
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
  fd1 = fopen( "mimo-a.iq", "w+b" );
  fd2 = fopen( "mimo-b.iq", "w+b" );
  if ( fd1 == NULL || fd2 == NULL ) perror("fopen()");
  int maxsize = 5; //     =1sec 
  while (maxsize--!=0){ /////////////////// start main loop                 
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
  LMS_Close(device1);
  fclose( fd1 );
  fclose( fd2 );
  return 0;
}
