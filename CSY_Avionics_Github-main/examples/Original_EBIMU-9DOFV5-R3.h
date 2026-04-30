////////////////////////////////////////////////////////////////////
/////////////////////// EBIMU FUNCTION /////////////////////////////
#define SBUF_SIZE 64
 
char sbuf[SBUF_SIZE];
signed int sbuf_cnt=0;
 
int EBimuAsciiParser(float *item, int number_of_item)
{
  int n,i;
  int rbytes;
  char *addr; 
  int result = 0;
  
  rbytes = Serial.available();
  for(n=0;n<rbytes;n++)
  {
    sbuf[sbuf_cnt] = Serial.read();
    if(sbuf[sbuf_cnt]==0x0a)
       {
           addr = strtok(sbuf,",");
           for(i=0;i<number_of_item;i++)
           {
              item[i] = atof(addr);
              addr = strtok(NULL,",");
           }
 
           result = 1;
       }
     else if(sbuf[sbuf_cnt]=='*')
       {   sbuf_cnt=-1;
       }
 
     sbuf_cnt++;
     if(sbuf_cnt>=SBUF_SIZE) sbuf_cnt=0;
  }
  
  return result;
}
/////////////////////// EBIMU FUNCTION /////////////////////////////
////////////////////////////////////////////////////////////////////
// 출처: https://www.e2box.co.kr/category/%EA%B8%B0%EC%88%A0%EC%A0%95%EB%B3%B4%20%EB%B0%8F%20%EC%9E%90%EB%A3%8C/EBIMU-9DOFV5
