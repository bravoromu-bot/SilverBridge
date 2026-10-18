#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>
static void show(MIDIObjectRef obj,const char *tag){
 CFStringRef name=NULL; MIDIObjectGetStringProperty(obj,kMIDIPropertyName,&name);
 char text[256]=""; if(name){CFStringGetCString(name,text,sizeof(text),kCFStringEncodingUTF8);CFRelease(name);}
 SInt32 hidden=-1,offline=-1,unique=-1;
 MIDIObjectGetIntegerProperty(obj,kMIDIPropertyPrivate,&hidden);
 MIDIObjectGetIntegerProperty(obj,kMIDIPropertyOffline,&offline);
 MIDIObjectGetIntegerProperty(obj,kMIDIPropertyUniqueID,&unique);
 printf("%s name=%s private=%d offline=%d unique=%d\n",tag,text,hidden,offline,unique);
 if (!strcmp(tag,"device")) {
  SInt32 value=0;
  if (!MIDIObjectGetIntegerProperty(obj,CFSTR("USBVendorProduct"),&value)) printf(" USBVendorProduct=%08x\n",(unsigned)value);
  if (!MIDIObjectGetIntegerProperty(obj,CFSTR("USBLocationID"),&value)) printf(" USBLocationID=%08x\n",(unsigned)value);
  const CFStringRef keys[]={CFSTR("PloytecVersion"),CFSTR("SerialNumber")};
  const char *labels[]={"PloytecVersion","SerialNumber"};
  for(int i=0;i<2;i++){CFStringRef val=NULL;if(!MIDIObjectGetStringProperty(obj,keys[i],&val)&&val){char buf[256]="";CFStringGetCString(val,buf,sizeof(buf),kCFStringEncodingUTF8);printf(" %s=%s\n",labels[i],buf);CFRelease(val);}}
 }
}
int main(void){MIDIClientRef client=0; OSStatus rc=MIDIClientCreate(CFSTR("Silver detail"),NULL,NULL,&client); if(rc){printf("client=%d\n",(int)rc);return 1;}
 for(ItemCount i=0;i<MIDIGetNumberOfDevices();i++){MIDIDeviceRef d=MIDIGetDevice(i);CFStringRef name=NULL;MIDIObjectGetStringProperty(d,kMIDIPropertyName,&name);char text[256]="";if(name){CFStringGetCString(name,text,sizeof(text),kCFStringEncodingUTF8);CFRelease(name);}if(strcmp(text,"MPC Studio"))continue;show(d,"device");ItemCount n=MIDIDeviceGetNumberOfEntities(d);printf("entities=%lu\n",(unsigned long)n);for(ItemCount e=0;e<n;e++){MIDIEntityRef ent=MIDIDeviceGetEntity(d,e);show(ent,"entity");ItemCount sn=MIDIEntityGetNumberOfSources(ent),dn=MIDIEntityGetNumberOfDestinations(ent);printf(" sources=%lu destinations=%lu\n",(unsigned long)sn,(unsigned long)dn);for(ItemCount s=0;s<sn;s++)show(MIDIEntityGetSource(ent,s),"  source");for(ItemCount x=0;x<dn;x++)show(MIDIEntityGetDestination(ent,x),"  destination");}}
 MIDIClientDispose(client);return 0;}
