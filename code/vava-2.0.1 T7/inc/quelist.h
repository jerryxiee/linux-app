#ifndef _QUEUE_LIST_H_
#define _QUEUE_LIST_H_

#define CMD_QUEUE_LEN	50
#define CMD_PARAM_LEN   90

//信令队列
typedef struct _cmd_data{
	int channel;
	int sessionid;
	int cmdtype;
	char param[CMD_PARAM_LEN];
}qCmdData;

typedef struct _cmd_qlist{
	qCmdData data[CMD_QUEUE_LEN];  //队列数组 
	int front;  //队头 
	int rear;  	//队尾 
}qCmdList, *pqCmdList;

//录像删除队列
typedef struct _rec_data{
	int type;
	char dirname[9];
	char filename[11];
}qRecData;

typedef struct _rec_qlist{
	qRecData data[CMD_QUEUE_LEN];  //队列数组 
	int front;  //队头 
	int rear;  	//队尾 
}qRecList, *pqRecList;

//图片和录像处理队列
typedef struct _img_data{
	int channel;
	int sessionid;
	char dirname[9];
	char filename[11];
}qImgData;

typedef struct _img_qlist{
	qImgData data[CMD_QUEUE_LEN];  //队列数组 
	int front;  //队头 
	int rear;  	//队尾 
}qImgList, *pqImgList;

//推送处理队列
typedef struct _push_data{
	int channel;
	int pushtype;
	int filetype;
	int time;
	int ntsamp;
	char dirname[9];
	char filename[11];
	char msg[20];
}qPushData;

typedef struct _push_qlist{
	qPushData data[CMD_QUEUE_LEN]; //队列数组
	int front;  //队头
	int rear;	//队尾
}qPushList, *pqPushList;

int CmdList_init();
int CmdList_IsEmpty();
int CmdList_IsFull();
int CmdList_InsertData(qCmdData data);
int CmdList_OutData(qCmdData *data);
void *CmdQueList_pth(void *data);

int RecList_init();
int RecList_IsEmpty();
int RecList_IsFull();
int RecList_InsertData(qRecData data);
int RecList_OutData(qRecData *data);
void *RecList_pth(void *data);

int ImgList_init();
int ImgList_IsEmpty();
int ImgList_IsFull();
int ImgList_InsertData(qImgData data);
int ImgList_OutData(qImgData *data);
void *ImgQueList_pth(void *data);

int PushList_init();
int PushList_IsEmpty();
int PushList_IsFull();
int PushList_InsertData(qPushData data);
int PushList_OutData(qPushData *data);
void *PushQueList_pth(void *data);

#endif

