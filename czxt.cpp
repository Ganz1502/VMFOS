#include<iostream>
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<algorithm>
#include<conio.h>
#include<fstream>
#include<string>
#include<sstream>
#include<iomanip>
#include<windows.h>
using namespace std;

#define USER_NUM 9//最多9个用户
#define SUM_MEM_SIZE 3*10*1024*1024//总磁盘空间为30M
#define MEM_SIZE 3*1024*1024//每个用户磁盘空间为3M
#define DISK_SIZE 1024//磁盘块大小1K
#define DISK_NUM 3*1024//磁盘块3K
#define MSD 7//最大子目录数max subdirect
#define FATSIZE DISK_NUM*sizeof(fatItem)//FAT表大小 sizeof是一种内存容量度量函数，功能是返回一个变量或者类型的大小
#define USER_ROOT_STARTBLOCK FATSIZE/DISK_SIZE+1//用户根目录起始盘块号
#define USER_ROOT_SIZE sizeof(direct)//用户根目录大小
#define MAX_FILE_SIZE 5000//最大文件长度

/*文件控制块*/
typedef struct FCB {
	char fileName[20];//文件或目录名
	int type;// 0--文件  1--目录 
	char* content = NULL;//文件内容
	int size;//大小
	int firstDisk;//起始盘块
	int next;//子目录起始盘块号
	int sign;//0--普通目录  1--根目录
}FCB;

/*目录*/
struct direct {
	FCB directItem[MSD + 2];
};

/*文件分配表*/
typedef struct fatItem {
	int item;//存放文件下一个磁盘的指针
	int state;//0-- 空闲  1--被占用
}fatItem, * myFAT;

/*用户表*/
typedef struct User {
	char name[20];//用户名
	char password[20];//用户密码
	int flag = 0;//标记是是否被初始化过
}person;
person* user;

/*打开文件表*/
struct table
{
	char fname[20];//文件名
	int cnt;//打开的次数
	int firdisk;//启示盘块号
	int read = 0;//读权限，拥有读权限后，可以读
	int write = 0;//写权限，拥有写权限后，可以追加和重写
}Table[FATSIZE];

int tnum = 0;//现在打开的文件的个数

string userName, userPassword;//当前用户名和密码
int now_usernum = 0;//现在有几个已经注册的用户
int now_user = 0;//我现在这个用户是第几个用户
myFAT fat;//我的myFAT表
direct* root;//当前用户根目录
direct* curDir;//当前用户当前目录
string curPath;//当前用户当前路径
char* fdisk;//虚拟磁盘起始地址
int flag_authority = 0;//权限，有些文件无法open就必须访问，需要标记


/*输出当前用户名和路径*/
void Shell()
{
	//设置用户名和路径的前景色和背景色
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
	cout << curPath << ">";//输出用户名和路径
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

}
/*保存磁盘数据*/
int save()
{
	ofstream fp;
	fp.open("os.dat", ios::binary);
	fp.write(fdisk, SUM_MEM_SIZE * sizeof(char));
	fp.close();
	return 1;
}
/*用户磁盘初始化*/
void startsys() //查看是否创建虚拟磁盘，初始化 
{
	char* buf; //缓冲区 
	user = (User*)(fdisk);//user从第一个块开始存
	fdisk = (char*)malloc(SUM_MEM_SIZE);
	memset(fdisk, 0, SUM_MEM_SIZE);
	fat = (fatItem*)(fdisk + MEM_SIZE);//前3M给用户
	ifstream fpr("os.dat", ios::binary);
	if (fpr)
	{
		fpr.read(fdisk, SUM_MEM_SIZE * sizeof(char));
		user = (User*)(fdisk);
		fpr.close();
	}
	else //创建虚拟磁盘 
	{
		fpr.close();
		ofstream fp("os.dat", ios::binary);
		person t;
		user = new User[10];
		strcpy(t.name, "");
		strcpy(t.password, "");
		t.flag = 0;
		int j;
		for (j = 0; j < USER_NUM; j++)
		{
			user[j] = t;
		}
		fp.write(fdisk, SUM_MEM_SIZE * sizeof(char));
		fp.close();
	}

}
void init()
{
	//size of返回一个对象或者类型所占的内存字节数。
	//初始化FAT表
	fat = (myFAT)(fdisk + DISK_SIZE + MEM_SIZE + now_user * MEM_SIZE);//fat地址
	fat[0].item = -1;
	fat[0].state = 1;//初始化FAT表
	for (int i = 1; i < USER_ROOT_STARTBLOCK - 1; i++)
	{
		fat[i].item = i + 1;
		fat[i].state = 1;
	}
	//以用户根目录启示盘号分割
	fat[USER_ROOT_STARTBLOCK].item = -1;
	fat[USER_ROOT_STARTBLOCK].state = 1;//根目录磁盘块号
	//fat其他块初始化，都是空闲的
	for (int i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++)
	{
		fat[i].item = -1;
		fat[i].state = 0;
	}
	//根目录初始化
	root = (struct direct*)(fdisk + DISK_SIZE + FATSIZE + now_user * MEM_SIZE + MEM_SIZE);//根目录地址
	root->directItem[0].sign = 1;
	root->directItem[0].firstDisk = USER_ROOT_STARTBLOCK + now_user * DISK_NUM + DISK_NUM;
	strcpy(root->directItem[0].fileName, ".");
	root->directItem[0].next = root->directItem[0].firstDisk;
	root->directItem[0].type = 1;
	root->directItem[0].size = USER_ROOT_SIZE;
	//父目录初始化
	root->directItem[1].sign = 1;
	root->directItem[1].firstDisk = USER_ROOT_STARTBLOCK + now_user * DISK_NUM + DISK_NUM;
	strcpy(root->directItem[1].fileName, "..");
	root->directItem[1].next = root->directItem[0].firstDisk;
	//cout << fat[25].item << " " << fat[26].item << " " << fat[27].item << endl;
	root->directItem[1].type = 1;
	root->directItem[1].size = USER_ROOT_SIZE;
	//子目录初始化
	for (int i = 2; i < MSD + 2; i++) {
		root->directItem[i].sign = 0;
		root->directItem[i].firstDisk = -1;
		strcpy(root->directItem[i].fileName, "");
		root->directItem[i].next = -1;
		root->directItem[i].type = 0;
		root->directItem[i].size = 0;
	}
	if (!save())
		cout << "初始化失败！" << endl;
}
/*读取磁盘数据内容——————只用来读用户名和密码登录用*/
void readDisk()
{
	fdisk = new char[SUM_MEM_SIZE];//申请磁盘大小缓冲区
	ifstream fp;
	fp.open("os.dat", ios::in | ios::binary);//以读的方式打开
	if (fp)
		fp.read(fdisk, SUM_MEM_SIZE);
	else
		cout << "打开失败！" << endl;
	fp.close();
	user = (User*)(fdisk);//读用户用户名和密码
}
/*读取用户磁盘数据内容——————用来读目前我登录的用户占的磁盘，获取其他信息*/
void readUserDisk()
{
	fdisk = new char[SUM_MEM_SIZE];//申请磁盘大小缓冲区
	ifstream fp;
	fp.open("os.dat", ios::in | ios::binary);//以读的方式打开
	if (fp)
		fp.read(fdisk, SUM_MEM_SIZE);
	else
		cout << "打开失败！" << endl;
	fp.close();
	fat = (myFAT)(fdisk + DISK_SIZE + now_user * MEM_SIZE + MEM_SIZE);//fat地址
	user = (User*)(fdisk);//读用户用户名和密码
	root = (struct direct*)(fdisk + DISK_SIZE + FATSIZE + now_user * MEM_SIZE + MEM_SIZE);//根目录地址
	curDir = root;
	curPath = userName + ":\\";
}
/*释放缓冲区空间*/
void release()
{
	delete fdisk;
}
/*打开文件*/
void open(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//检查该目录下是否有目标文件
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2)
	{
		cout << "找不到该文件！" << endl;
		return;
	}
	int flag = 0;
	int tnum2;
	for (j = 0; j < tnum; j++)
	{
		if (!(strcmp(Table[j].fname, fName)))
		{
			flag = 1;
			Table[j].cnt++;
			tnum2 = j;
		}
	}
	if (flag == 0)
	{
		Table[tnum].cnt = 1;
		Table[tnum].firdisk = curDir->directItem[i].firstDisk;
		strcpy(Table[tnum].fname, curDir->directItem[i].fileName);
		tnum2 = tnum;
	}
	cout << "*************************************" << endl << endl;
	cout << "请选择您打开该文件的权限：" << endl;
	cout << "1.只读          2.只写         3.读写" << endl << endl;
	cout << "*************************************" << endl;
	int a;
	cin >> a;
	while (1)
	{
		if (a == 1)
		{
			Table[tnum2].read = 1;
			break;
		}
		else if (a == 2)
		{
			Table[tnum2].write = 1;
			break;
		}
		else if (a == 3)
		{
			Table[tnum2].read = 1;
			Table[tnum2].write = 1;
			//cout << "权限：" << Table[tnum].write << Table[tnum].read<< endl;
			//cout << tnum << endl;
			break;
		}
		else
		{
			cout << "输入有误！请您重新输入：" << endl;
			cin >> a;
		}
	}
	cout << fName << "已经打开！" << endl;
	if (flag == 0)
		tnum++;
	cout << "现在打开的文件的数目是：" << tnum << endl;
	getchar();
}
void close(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//检查该目录下是否有目标文件
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2)
	{
		cout << "找不到该文件！" << endl;
		return;
	}
	int flag = 0;
	for (j = 0; j < tnum; j++)
	{
		if (!(strcmp(Table[j].fname, fName)) && Table[j].cnt >= 1)
		{
			flag = 1;
			Table[j].cnt--;
			if (Table[j].cnt == 0)
			{
				for (int k = j; k < tnum - 1; k++)
				{
					Table[k] = Table[k + 1];
				}
				tnum -= 1;
			}
			break;
		}
	}
	if (flag == 0)
	{
		cout << "该文件未被打开过！" << endl;
	}
	else cout << "该文件已关闭！" << endl;
	cout << "现在仍然打开的文件的数目是：" << tnum << endl;
}
void write(string* str, char* content, int size)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//在当前目录下查找目标文件
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			//名字相同 并且是文件 就算找到了
			break;
	if (i >= MSD + 2)
	{
		cout << "找不到该文件！" << endl;
		return;
	}
	strcpy(fName, str[1].c_str());
	//检查该目录下是否有目标文件
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2)
	{
		cout << "找不到该文件！" << endl;
		return;
	}
	if (flag_authority == 0)
	{
		int flag = 0;
		int write = 0;
		for (j = 0; j < tnum; j++)
		{
			if (!(strcmp(Table[j].fname, fName)) && Table[j].cnt >= 1)
			{
				flag = 1;
				write = Table[j].write;
				break;
			}
		}
		if (flag == 0)
		{
			cout << "该文件还没有open！无法进行其他操作" << endl;
		}
		else if (write == 0)
		{
			cout << "该文件没有写的权限！" << endl;
		}
		else
			flag_authority = 1;
	}
	if(flag_authority ==1)
	{
		flag_authority = 0;
		int cur = i;//当前目录项的下标
		int fSize = curDir->directItem[cur].size;//目标文件大小
		int item = curDir->directItem[cur].firstDisk;//目标文件的起始磁盘块号
		int item2 = item - (now_user + 1) * DISK_NUM;
		while (fat[item2].item != -1)
			item = fat[item2].item;//计算保存该文件的最后一块盘块号*/
		char* first = fdisk + item * DISK_SIZE + fSize % DISK_SIZE;//计算该文件的末地址
		//如果盘块剩余部分够写，则直接写入剩余部分
		if (DISK_SIZE - fSize % DISK_SIZE > size)
		{
			strcpy(first, content);
			curDir->directItem[cur].size += size;
		}
		//如果盘块剩余部分不够写，则找到空闲磁盘块写入
		else
		{
			//先将起始磁盘剩余部分写完
			for (j = 0; j < DISK_SIZE - fSize % DISK_SIZE; j++)
			{
				first[j] = content[j];
			}

			int res_size = size - (DISK_SIZE - fSize % DISK_SIZE);//剩余要写的内容大小
			int needDisk = res_size / DISK_SIZE;//占据的磁盘块数量
			int needRes = res_size % DISK_SIZE;//占据最后一块磁盘块的大小

			if (needRes > 0)
				needDisk += 1;
			for (j = 0; j < needDisk; j++)
			{
				for (i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++)
					if (fat[i].state == 0)
						break;
				if (i >= DISK_NUM)
				{
					cout << "磁盘已被分配完！" << endl;
					return;
				}
				first = fdisk + i * DISK_SIZE + (now_user + 1) * DISK_NUM;//空闲磁盘起始盘物理地址
				//当写到最后一块磁盘，则只写剩余部分内容
				if (j == needDisk - 1)
				{
					for (int k = 0; k < size - (DISK_SIZE - fSize % DISK_SIZE - j * DISK_SIZE); k++)
						first[k] = content[k];
				}
				else {
					for (int k = 0; k < DISK_SIZE; k++)
						first[k] = content[k];
				}
				//修改文件分配表内容
				fat[item2].item = i;
				fat[i].state = 1;
				fat[i].item = -1;
			}
			curDir->directItem[cur].size += size;
		}
	}
}
/*touch创建文件*/
int touch(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//检查文件名是否合法
	if (!strcmp(fName, ""))return 0;
	if (str[1].length() > 20) {
		cout << "文件名太长！请重新输入！" << endl;
		return 0;
	}
	//找当前目录下是否有文件重名
	for (int pos = 2; pos < MSD + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0) {
			cout << "当前目录下已存在文件重名!" << endl;
			return 0;
		}
	}
	//检查当前目录下空间是否已满
	int i;
	for (i = 2; i < MSD + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= MSD + 2) {
		cout << "当前目录下空间已满" << endl;
		return 0;
	}
	//检查是否有空闲磁盘块
	int j;
	for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= DISK_NUM) {
		cout << "无空闲盘块！" << endl;
		return 0;
	}
	//文件初始化
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j + now_user * DISK_NUM + DISK_NUM;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].next = j + now_user * DISK_NUM + DISK_NUM;
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	return 1;
}
/*显示当前目录下子目录和文件信息*/
void dir()
{
	cout << fixed << left << setw(20) << "文件名" << setw(10) << "类型" << setw(10) << "大小</B>" << setw(10) << "起始磁盘块号" << endl;
	for (int i = 2; i < MSD + 2; i++) {
		if (curDir->directItem[i].firstDisk != -1)
		{
			cout << fixed << setw(20) << curDir->directItem[i].fileName;
			if (curDir->directItem[i].type == 0) {//显示文件
				cout << fixed << setw(10) << " " << setw(10) << curDir->directItem[i].size;
			}
			else {//显示目录
				cout << fixed << setw(10) << "<DIR>" << setw(10) << " ";
			}
			cout << setw(10) << curDir->directItem[i].firstDisk << endl;
		}
	}
}
/*创建目录*/
int mkdir(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//检查目录名是否合法
	if (!strcmp(fName, ""))return 0;
	if (str[1].length() > 10) {
		cout << "目录名太长！请重新输入！" << endl;
		return 0;
	}
	if (!strcmp(fName, ".") || !strcmp(fName, "..")) {
		cout << "目录名称不合法！" << endl;
		return 0;
	}
	//找当前目录下是否有目录重名
	for (int pos = 2; pos < MSD + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 1) {
			cout << "当前目录下已存在目录重名!" << endl;
			return 0;
		}
	}
	//检查当前目录下空间是否已满
	int i;
	for (i = 2; i < MSD + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= MSD + 2) {
		cout << "当前目录下空间已满" << endl;
		return 0;
	}
	//检查是否有空闲磁盘块
	int j;
	for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= DISK_NUM) {
		cout << "无空闲盘块！" << endl;
		return 0;
	}
	//创建目录初始化
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j + now_user * DISK_NUM + DISK_NUM;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].next = j + now_user * DISK_NUM + DISK_NUM;
	curDir->directItem[i].type = 1;
	curDir->directItem[i].size = USER_ROOT_SIZE;
	direct* cur_mkdir = (direct*)(fdisk + curDir->directItem[i].firstDisk * DISK_SIZE);//创建目录的物理地址
	//指向当前目录的目录项
	cur_mkdir->directItem[0].sign = 0;
	cur_mkdir->directItem[0].firstDisk = curDir->directItem[i].firstDisk;
	strcpy(cur_mkdir->directItem[0].fileName, ".");
	cur_mkdir->directItem[0].next = cur_mkdir->directItem[0].firstDisk;
	cur_mkdir->directItem[0].type = 1;
	cur_mkdir->directItem[0].size = USER_ROOT_SIZE;
	//指向上一级目录的目录项
	cur_mkdir->directItem[1].sign = curDir->directItem[0].sign;
	cur_mkdir->directItem[1].firstDisk = curDir->directItem[0].firstDisk;
	strcpy(cur_mkdir->directItem[1].fileName, "..");
	cur_mkdir->directItem[1].next = cur_mkdir->directItem[1].firstDisk;
	cur_mkdir->directItem[1].type = 1;
	cur_mkdir->directItem[1].size = USER_ROOT_SIZE;
	//子目录初始化
	for (int i = 2; i < MSD + 2; i++) {
		cur_mkdir->directItem[i].sign = 0;
		cur_mkdir->directItem[i].firstDisk = -1;
		strcpy(cur_mkdir->directItem[i].fileName, "");
		cur_mkdir->directItem[i].next = -1;
		cur_mkdir->directItem[i].type = 0;
		cur_mkdir->directItem[i].size = 0;
	}
	return 1;
}
/*删除目录*/
int rmdir(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//在当前目录下查询目标目录
	int i, j; direct* tempDir = curDir;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(tempDir->directItem[i].fileName, fName) && tempDir->directItem[i].type == 1)
			break;
	if (i >= MSD + 2) {
		cout << "找不到该目录！" << endl;
		return 0;
	}
	//检查是否有子目录或文件
	tempDir = (direct*)(fdisk + tempDir->directItem[i].next * DISK_SIZE);
	for (j = 2; j < MSD + 2; j++) {
		if (tempDir->directItem[j].next != -1) {
			cout << "删除失败！该目录下有子目录或文件!" << endl;
			return 0;
		}
	}
	int item = curDir->directItem[i].firstDisk;//目标文件的起始磁盘块号
	int item2 = item - (now_user + 1) * DISK_NUM;
	fat[item2].state = 0;//修改文件分配表
	//修改目录项

	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].next = -1;
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	return 1;
}
/*删除文件*/
int del(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//在该目录下找目标文件
	int i, temp;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "找不到该文件！" << endl;
		return 0;
	}

	int item = curDir->directItem[i].firstDisk;//目标文件的起始磁盘块号
	int item2 = item - (now_user + 1) * DISK_NUM;
	//根据文件分配表依次删除文件所占据的磁盘块
	while (item2 != -1)
	{
		temp = fat[item2].item;
		fat[item2].item = -1;
		fat[item2].state = 0;
		item2 = temp;
	}
	//释放目录项
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].next = -1;
	curDir->directItem[i].type = 0;
	//	curDir->directItem[i].content=NULL;
	//	cout << curDir->directItem[i].content<< endl;
	curDir->directItem[i].size = 0;
	//cout << "测试删除是否执行" << endl;
	return 1;
}
/*切换目录*/
void cd(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	string objPath = str[1];//目标路径
	if (objPath[objPath.size() - 1] != '\\')objPath += "\\";
	int start = -1;
	int end = 0;//以\为分割，每段起始下标和结束下标
	int i, j, len = objPath.length();
	direct* tempDir = curDir;
	string oldPath = curPath;//保存切换前的路径
	string temp;
	for (i = 0; i < len; i++) {
		//如果目标路径以\开头，则从根目录开始查询
		if (objPath[0] == '\\') {
			tempDir = root;
			curPath = userName + ":\\";
			continue;
		}
		if (start == -1)
			start = i;
		if (objPath[i] == '\\') {
			end = i;
			temp = objPath.substr(start, end - start);
			//检查目录是否存在
			for (j = 0; j < MSD + 2; j++)
				if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
					break;
			if (j >= MSD + 2) {
				curPath = oldPath;
				cout << "找不到该目录！" << endl;
				return;
			}
			//如果目标路径为".."，则返回上一级
			if (temp == "..") {
				if (tempDir->directItem[j - 1].sign != 1) {
					int pos = curPath.rfind('\\', curPath.length() - 2);
					curPath = curPath.substr(0, pos + 1);
				}
			}
			//如果目标路径为"."，则指向当前目录
			else {
				if (temp != ".")
					curPath = curPath + objPath.substr(start, end - start) + "\\";
			}
			start = -1;
			//cout << tempDir->directItem[j].firstDisk << "!!" << endl;
			tempDir = (direct*)(fdisk + tempDir->directItem[j].firstDisk * DISK_SIZE);
			//cout << tempDir->directItem[j].firstDisk << "!!" << endl;

		}
	}
	curDir = tempDir;
}
//显示文件内容
int more(string* str, char* buf)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//检查该目录下是否有目标文件
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "找不到该文件！" << endl;
		return 0;
	}
	if (flag_authority == 0)
	{
		int flag = 0;
		int read2 = 0;
		for (j = 0; j < tnum; j++)
		{
			if (!(strcmp(Table[j].fname, fName)) && Table[j].cnt >= 1)
			{
				flag = 1;
				read2 = Table[j].read;
				break;
			}
		}
		if (flag == 0)
		{
			cout << "该文件还没有open！无法进行其他操作" << endl;
			return 0;
		}
		else if (read2 == 0)
		{
			cout << "该文件没有读的权限！" << endl;
			return 0;
		}
		else
			flag_authority = 1;
	}
	if(flag_authority ==1)
	{
		flag_authority = 0;
		int item = curDir->directItem[i].firstDisk;//目标文件的起始磁盘块号
		//cout << "测试 目标文件盘号" << curDir->directItem[i].firstDisk << endl;
		int fSize = curDir->directItem[i].size;//目标文件的大小
		//cout << "测试 目标文件大小" << fSize << endl;
		int needDisk = fSize / DISK_SIZE;//占据的磁盘块数量
		//cout << "测试 目标文件磁盘块数量" << needDisk << endl;
		int needRes = fSize % DISK_SIZE;//占据最后一块磁盘块的大小
		//cout << "测试 目标文件最后一块磁盘号大小" << needRes << endl;
		if (needRes > 0)
			needDisk += 1;
		char* first = fdisk + item * DISK_SIZE;//起始磁盘块的物理地址
		int item2 = item - (now_user + 1) * DISK_NUM;
		//将目录文件内容拷贝到缓冲区中
		if (fSize <= 0)
		{
			buf[0] = '\0';
			return 0;
		}
		for (i = 0; i < needDisk; i++)
		{
			for (j = 0; j < fSize - i * DISK_SIZE; j++)
			{
				buf[i * DISK_SIZE + j] = first[j];
			}
			if (i == needDisk - 1 && j == fSize - i * DISK_SIZE)
				buf[i * DISK_SIZE + j] = '\0';
			if (i != needDisk - 1)//没有读到最后一个块的时候，找下一个要读的块
			{
				item = fat[item2].item;
				first = fdisk + item * DISK_SIZE;
			}
		}
		return 1;
	}
}//
/*复制文件或空目录子函数*/
void xcopy_fun(direct* sourceDir, direct* objDir)
{
	//遍历子目录和文件
	int i;
	for (int k = 2; k < MSD + 2; k++) {
		//如果是目录，复制目录并重复调用本函数
		if (sourceDir->directItem[k].type == 1) {
			curDir = objDir;
			string newStr[3]; newStr[0] = "mkdir"; newStr[1] = sourceDir->directItem[k].fileName;
			mkdir(newStr);
			direct* source = (direct*)(fdisk + sourceDir->directItem[k].next * DISK_SIZE);
			for (i = 2; i < MSD + 2; i++) {
				if (!strcmp(curDir->directItem[i].fileName, sourceDir->directItem[k].fileName) && curDir->directItem[i].type == 1)
					break;
			}
			direct* obj = (direct*)(fdisk + curDir->directItem[i].next * DISK_SIZE);
			xcopy_fun(source, obj);
		}
		//如果是文件，则复制文件到目路径标
		else {
			string newStr[3]; newStr[0] = "touch"; newStr[1] = sourceDir->directItem[k].fileName;
			if (newStr[1] == "")continue;
			//读文件
			char* content = new char[MAX_FILE_SIZE];//文件内容
			curDir = sourceDir;
			more(newStr, content);
			curDir = objDir;
			//创建文件并复制
			if (touch(newStr))
				write(newStr, content, strlen(content));
			delete[] content;
		}
	}
}
/*复制文件或空目录*/
void copy(string* str)
{
	char source[20]; 
	strcpy(source, str[1].c_str()); string s_source = str[1];//原文件或目录
	char obj[20];
	strcpy(obj, str[2].c_str()); string s_obj = str[2];//目标文件或目录
	//检查原文件或目录是否存在并合法
	if (s_source == "" || s_obj == "") {
		cout << "输入错误！" << endl;
		return;
	}
	int i, j;
	for (i = 2; i < MSD + 2; i++) {
		if (!strcmp(curDir->directItem[i].fileName, source))
			break;
	}
	if (i >= MSD + 2) {
		cout << "找不到源文件！" << endl;
		return;
	}
	direct* cur = curDir;//保存当目录
	string oldPath = curPath;//保存当前路径
	string newStr[3];
	newStr[0] = "touch";
	char* content = new char[MAX_FILE_SIZE];
	//如果原文件类型为"文件"
	flag_authority = 1;
	if (curDir->directItem[i].type == 0)
	{
		//读取源文件内容
		more(str, content);
	}
	//根据目标文件或目录路径切换当前目录
	if (s_obj[s_obj.length() - 1] == '\\') {
		string objPath[2]; objPath[0] = "cd";
		newStr[1] = s_source;
		objPath[1] = s_obj;
		cd(objPath);
	}
	else {
		string objPath[2]; objPath[0] = "cd";
		int pos = s_obj.rfind('\\', s_obj.length() - 1);
		if (pos != -1) {
			newStr[1] = s_obj.substr(pos + 1, s_obj.length() - 1);
			objPath[1] = s_obj.substr(0, pos + 1);
			cd(objPath);
		}
		else {
			newStr[1] = s_obj;
		}
	}
	//如果原文件类型为"文件"
	if (cur->directItem[i].type == 0) {
		if (touch(newStr))
		{
			write(newStr, content, strlen(content));
			cout << "复制成功!" << endl;
		}
		else
			cout << "复制失败!" << endl;
	}
	//如果原文件类型为"目录"
	if (cur->directItem[i].type == 1) {
		direct* tempDir = (direct*)(fdisk + cur->directItem[i].next * DISK_SIZE);
		for (j = 2; j < MSD + 2; j++) {
			if (tempDir->directItem[j].next != -1) {
				cout << "该目录下有目录或文件！" << endl;
				break;
			}
		}
		mkdir(newStr);
	}
	curDir = cur;
	curPath = oldPath;
	delete[] content;
}
/*复制子目录和文件*/
void xcopy(string* str)
{
	char source[20]; strcpy(source, str[1].c_str()); string s_source = str[1];//原文件或目录
	char obj[20]; strcpy(obj, str[2].c_str()); string s_obj = str[2];//目标文件或目录
	string oldPath = curPath;
	flag_authority = 1;
	//检查原文件或目录是否存在并合法
	if (s_source == "" || s_obj == "") {
		cout << "输入错误！" << endl;
		return;
	}
	int i;
	for (i = 2; i < MSD + 2; i++) {
		if (!strcmp(curDir->directItem[i].fileName, source))
			break;
	}
	if (i >= MSD + 2) {
		cout << "找不到原文件！" << endl;
		return;
	}
	direct* cur = curDir;//保存当前路径
	//复制文件
	if (cur->directItem[i].type == 0)
	{
		copy(str);
	}
	//复制目录
	else
	{
		//根据目标文件或目录路径切换当前目录
		string newStr[3]; newStr[0] = "touch";
		if (s_obj[s_obj.length() - 1] == '\\') {
			string objPath[2]; objPath[0] = "cd";
			newStr[1] = s_source;
			objPath[1] = s_obj;
			cd(objPath);
		}
		else {
			string objPath[2]; objPath[0] = "cd";
			int pos = s_obj.rfind('\\', s_obj.length() - 1);
			if (pos != -1) {
				newStr[1] = s_obj.substr(pos + 1, s_obj.length() - 1);
				objPath[1] = s_obj.substr(0, pos + 1);
				cd(objPath);
			}
			else {
				newStr[1] = s_obj;
			}
		}
		//调用xcopy子函数，复制子目录所有文件
		direct* sourceDir = (direct*)(fdisk + cur->directItem[i].next * DISK_SIZE);//源目录
		if (mkdir(newStr)) {
			for (i = 2; i < MSD + 2; i++) {
				if (!strcmp(curDir->directItem[i].fileName, newStr[1].c_str()))
					break;
			}
			direct* objDir = (direct*)(fdisk + curDir->directItem[i].next * DISK_SIZE);//目标目录
			xcopy_fun(sourceDir, objDir);
		}
	}
	curDir = cur;
	curPath = oldPath;
}
/*查找当前目录下有目标内容的文件*/
void find(char* obj, int& flag)
{
	for (int i = 2; i < MSD + 2; i++) {
		//如果是有效文件，则检查是否有目标内容
		if (curDir->directItem[i].firstDisk != -1 && curDir->directItem[i].type == 0 && curDir->directItem[i].size != 0 && strcmp(curDir->directItem[i].fileName, "") != 0) {
			char* buf = new char[MAX_FILE_SIZE];
			string newStr[2]; newStr[0] = "more"; newStr[1] = curDir->directItem[i].fileName;
			more(newStr, buf);
			if (strstr(buf, obj) != NULL) {
				cout << "# " << curPath << newStr[1] << "中有目标内容！" << endl;
				flag = 1;
			}
			delete[] buf;
		}
		//如果是目录，则递归查找子目录下文件内容
		if (curDir->directItem[i].firstDisk != -1 && curDir->directItem[i].type == 1 && strcmp(curDir->directItem[i].fileName, "") != 0) {
			string dir = curDir->directItem[i].fileName;
			direct* cur = curDir;
			string oldPath = curPath;
			curDir = (direct*)(fdisk + curDir->directItem[i].next * DISK_SIZE);
			curPath = curPath + dir + "\\";
			find(obj, flag);
			curPath = oldPath;
			curDir = cur;
		}
	}
}
/*导入本地文件到磁盘*/
void import(string* str)
{
	flag_authority = 1;
	string filePath = str[1];
	char* content = new char[MAX_FILE_SIZE];//设置缓冲区
	fstream fp;
	fp.open(filePath, ios::in);
	string buf;
	if (fp) {
		while (fp) {
			buf.push_back(fp.get());}
		strcpy(content, buf.c_str());
		fp.close();
		int pos = filePath.rfind('\\', filePath.length() - 1);
		string obj = filePath.substr(pos + 1, filePath.length() - pos - 1);
		string newStr[2];newStr[0] = "touch";newStr[1] = obj;
		if (touch(newStr))//读取本地文件内容并创建文件写入
			write(newStr, content, strlen(content));
		delete[] content;
		cout << "导入文件成功" << endl;
	}
	else {
		cout << "打开本地文件失败！" << endl;
	}
}
/*导出磁盘中文件到本地*/
void Export(string* str)
{
	flag_authority = 1;
	string source = str[1], filePath = str[2];
	char* content = new char[MAX_FILE_SIZE];//设置缓冲区
	more(str, content);//读取文件内容
	fstream fp;
	fp.open(filePath, ios::out);
	if (fp) {
		fp << content;
		cout << "导出文件成功！" << endl;
	}
	else {
		cout << "导出文件失败" << endl;
	}
	delete[] content;
}
/*重写文件内容*/
void rewrite(string* str)
{
	if (del(str) && touch(str)) {
		//检查当前目录下是否有目标文件
		int i;
		for (i = 2; i < MSD + 2; i++)
			if (!strcmp(curDir->directItem[i].fileName, str[1].c_str()) && curDir->directItem[i].type == 0)
				break;
		if (i >= MSD + 2)
		{
			cout << "找不到该文件！" << endl;
			return;
		}
		char fName[20];
		strcpy(fName, str[1].c_str());
		if (flag_authority == 0)
		{
			int flag = 0;
			int write2 = 0;
			for (int j = 0; j < tnum; j++)
			{
				if (!(strcmp(Table[j].fname, fName)) && Table[j].cnt >= 1)
				{
					flag = 1;
					write2 = Table[j].write;
					break;
				}
			}
			if (flag == 0)
			{
				cout << "该文件还没有open！无法进行其他操作" << endl;
			}
			else if (write2 == 0)
			{
				cout << "该文件没有写的权限！" << endl;
			}
			else
				flag_authority = 1;
		}
		if(flag_authority == 1)
		{
			//输入写入内容，并以#号结束
			char ch;
			char content[MAX_FILE_SIZE];//设置缓冲区
			int size = 0;
			cout << "请输入文件内容，并以'#'为结束标志" << endl;
			while (1)
			{
				ch = getchar();
				if (ch == '#')break;
				if (size >= MAX_FILE_SIZE)
				{
					cout << "输入文件内容过长！" << endl;
					break;
				}
				content[size] = ch;
				size++;
			}
			getchar();
			write(str, content, size);
		}
	}
}

/*帮助栏*/
void help()
{
	cout << fixed << left;
	cout << "********************  帮助  ********************" << endl << endl;
	cout << setw(40) << "cd 路径" << setw(10) << "切换目录" << endl;
	cout << setw(40) << "create 文件名" << setw(10) << "创建文件" << endl;
	cout << setw(40) << "delete 文件名" << setw(10) << "删除文件" << endl;
	cout << setw(40) << "write 文件名" << setw(10) << "写入内容（追加模式，覆盖模式）" << endl;
	cout << setw(40) << "mkdir 目录名" << setw(10) << "创建目录" << endl;
	cout << setw(40) << "rmdir 目录名" << setw(10) << "删除目录" << endl;
	cout << setw(40) << "dir" << setw(10) << "显示当前目录下所有子目录和子文件" << endl;
	cout << setw(40) << "copy 源文件 目标文件" << setw(10) << "复制文件" << endl;
	cout << setw(40) << "xcopy 源目录 目标目录" << setw(10) << "复制目录" << endl;
	cout << setw(40) << "open" << setw(10) << "打开文件" << endl;
	cout << setw(40) << "close" << setw(10) << "关闭文件" << endl;
	cout << setw(40) << "head  -num" << setw(10) << "显示文件的前num行" << endl;
	cout << setw(40) << "tail  -num" << setw(10) << "显示文件尾巴上的num行" << endl;
	cout << setw(40) << "import 本地文件路径" << setw(10) << "导入文件" << endl;
	cout << setw(40) << "export 文件名 本地文件路径" << setw(10) << "导出文件" << endl;
	cout << setw(40) << "read 文件名" << setw(10) << "显示文件内容" << endl;
	cout << setw(40) << "save" << setw(10) << "保存磁盘数据" << endl;
	cout << setw(40) << "help" << setw(10) << "帮助文档" << endl;
	cout << setw(40) << "clear" << setw(10) << "清屏" << endl;
	cout << setw(40) << "exit" << setw(10) << "退出" << endl << endl;
}
/*菜单栏*/
void menu()
{
	//system("cls");
	getchar();
	cout << "***************** Hello  " << userName << "! *****************" << endl;
	readUserDisk();//读取磁盘中用户的位置
	//cout<<"测试init" <<user[now_user].flag <<endl;
	if (user[now_user].flag == 0)//如果没有初始化
	{
		init();//用户的初始化
		user[now_user].flag = 1;//标记为初始化过
		save();//保存
	}
	while (true)
	{
		Shell();
		//以空格为界线，读取命令,并用stringstream流分割命令
		string cmd;
		getline(cin, cmd);
		stringstream stream;
		stream.str(cmd);
		string str[5];
		for (int i = 0; stream >> str[i]; i++);
		//命令菜单选项
		if (str[0] == "exit") {
			exit(0);
		}
		else if (str[0] == "create") {
			if (touch(str))
				cout << "文件创建成功！" << endl;
		}
		else if (str[0] == "dir") {
			dir();
		}
		else if (str[0] == "mkdir") {
			if (mkdir(str))
				cout << str[1] << "创建成功！" << endl;
		}
		else if (str[0] == "open") {
			open(str);
		}
		else if (str[0] == "close") {
			close(str);
		}
		else if (str[0] == "read")
		{
			char* buf = new char[MAX_FILE_SIZE];//设置缓冲区
			if (more(str, buf) == 0)
			{
				cout << "*************************************" << endl;
				cout << "很抱歉！读取失败！" << endl;
				cout << "*************************************" << endl;
			}
			else
			{
				cout << "*************************************" << endl;
				if (strlen(buf))
					cout << buf << endl;
				cout << "*************************************" << endl;
			}
			delete[] buf;
		}
		else if (str[0] == "delete") {
			if (del(str))
				cout << str[1] << "删除成功！" << endl;
		}
		else if (str[0] == "rmdir") {
			if (rmdir(str))
				cout << str[1] << "删除成功！" << endl;
		}
		else if (str[0] == "write") {
			int choice;
			cout << "***********************模式选择**********************" << endl;
			cout << "1.追加模式                   2.覆盖模式               " << endl << endl;
			cout << "----------------------------------------------------" << endl;
			cout << "请输入您的选择：" << endl;
			cout << "****************************************************" << endl << endl;
			cin >> choice;
			getchar();
			if (choice == 1)
			{
				//检查当前目录下是否有目标文件
				int i;
				for (i = 2; i < MSD + 2; i++)
					if (!strcmp(curDir->directItem[i].fileName, str[1].c_str()) && curDir->directItem[i].type == 0)
						break;
				if (i >= MSD + 2) {
					cout << "找不到该文件！" << endl;
					continue;
				}
				//输入写入内容，并以#号结束
				char ch;
				char content[MAX_FILE_SIZE];//设置缓冲区
				int size = 0;//这个文件的大小
				cout << "请输入文件内容，并以'#'为结束标志" << endl;
				while (1)
				{
					ch = getchar();
					if (ch == '#')
					{
						ch = '\0';
						break;
					}
					if (size >= MAX_FILE_SIZE) {
						cout << "输入文件内容过长！" << endl;
						break;
					}
					content[size] = ch;
					//cout << size <<ch << endl;
					size++;
				}
				if (size >= MAX_FILE_SIZE)
					continue;
				getchar();
				write(str, content, size);
			}
			else if (choice == 2)
			{
				rewrite(str);
			}
			else
			{
				cout << "您的输入有误！" << endl;
			}
		}
		else if (str[0] == "cd") {
			cd(str);
		}
		else if (str[0] == "copy") {
			copy(str);
		}
		else if (str[0] == "xcopy") {
			xcopy(str);
		}
		else if (str[0] == "import") {
			import(str);
		}
		else if (str[0] == "export") {
			Export(str);
		}
		else if (str[0] == "head") {//前num行
			char* buf = new char[MAX_FILE_SIZE];//设置缓冲区
			more(str, buf);
			if (more(str, buf) == 0)
			{
				cout << "*************************************" << endl;
				cout << "很抱歉！读取失败！" << endl;
				cout << "*************************************" << endl;
			}
			else
			{
				cout << "*************************************" << endl;
				int i = 0;
				int num1 = 1;
				int n = (atoi)(str[2].c_str());
				int num2 = 0;
				while (i <= strlen(buf))
				{
				//	cout << i << ":" << buf[i] << " " << (int)buf[i] << endl;
					if (int(buf[i]) == 10)
						num1++;
					if (num1 == n)
						num2 = i;
					i++;
				}
				buf[num2 + 1] = '\0';
				cout << buf << endl;
				cout << "*************************************" << endl;
				delete[] buf;
			}
		}
		else if (str[0] == "tail") {//后num行
			char* buf = new char[MAX_FILE_SIZE];//设置缓冲区
			char* buf2 = new char[MAX_FILE_SIZE];//设置缓冲区
			more(str, buf);
			if (more(str, buf) == 0)
			{
				cout << "*************************************" << endl;
				cout << "很抱歉！读取失败！" << endl;
				cout << "*************************************" << endl;
			}
			else
			{
				cout << "*************************************" << endl;
				int i = 0;
				int j = 0;
				int num1 = 0;
				int num3 = 1;
				int n = (atoi)(str[2].c_str());
				int num2 = 0;
				while (i <= strlen(buf))
				{
					if (int(buf[i]) == 10)
						num1++;
					i++;
				}
				i = 0;
				while (i <= strlen(buf))
				{
					if (int(buf[i]) == 10)
						num3++;
					if (num3 == (num1 - n+1))
						num2 = i;
					i++;
				}
				for (i = num2 + 2; i <= strlen(buf); i++)
				{
					buf2[j] = buf[i];
					j++;
				}
				buf2[j] = '\0';
				//cout <<"n"<<n << endl;
				//cout << "num1" << num1 << endl;
				//cout << "num2" << num2 << endl;
				//cout << "num3" << num3 << endl;
				//cout << "测试" << (num1 - n) << endl;
				cout << buf2 << endl;
				cout << "*************************************" << endl;
				delete[] buf;
			}
		}
		else if (str[0] == "save") {
			if (save())
				cout << "保存成功！" << endl;
			else
				cout << "保存失败" << endl;
		}
		else if (str[0] == "clear") {
			system("cls");//清屏
		}
		else if (str[0] == "help") {
			help();//帮助文档
		}
		else {
			cout << "找不到该命令！请重新输入" << endl;
		}
	}
	release();
}
/*用户注册功能*/
void Register()
{
	string input;
	int f = 0;
	char name[20], word[20];
	int i = 0;
	cout << "用户名：";
	cin >> name;
	cout << "密码：";
	cin >> word;
	while (strcmp(user[i].name, "") != 0)
	{
		cout << user[i].name << endl;
		if (strcmp(user[i].name, name) == 0)
		{
			break;
		}
		i++;
	}
	if (i > USER_NUM)
	{
		cout << "用户数已达上限！" << endl;
	}
	if (strcmp(user[i].name, "") == 0)
	{
		strcpy(user[i].name, name);
		strcpy(user[i].password, word);
		f = 1;
		//LOGIN = 1;
		cout << "注册成功！" << endl;
		save();
	}
	else
		cout << "用户名已存在！" << endl;
}
/*用户登录功能*/
void Login()
{
	string input;
	int f = 0;
	char name[20], word[20];
	cout << "用户名：";
	cin >> name;
	cout << "密码：";
	cin >> word;
	int i = 0;
	while (strcmp(user[i].name, "") != 0)
	{
		//cout << user[i].name << " " << user[i].flag << endl;
		if (strcmp(user[i].name, name) == 0 && strcmp(user[i].password, word) == 0)
		{
			f = 1;
			now_user = i;
			//LOGIN = 1;
			cout << "登陆成功！" << endl;
			userName = name;
			menu();
			break;
		}
		i++;
	}
	if (f == 0)
		cout << "用户名或密码错误！" << endl;
}
/*主函数（开始界面）*/
int main()
{
	system("color 0B");
	while (true)
	{
		startsys();//总的磁盘 建立
		readDisk();//duqu
		cout << "****************欢迎进入cjb的文件系统****************" << endl;
		cout << "----------------------------------------------------" << endl;
		cout << "****************************************************" << endl << endl;
		cout << "1、登录               2、注册              0、退出" << endl << endl;
		cout << "****************************************************" << endl;
		cout << "请输入：";
		int choice;
		cin >> choice;
		switch (choice)
		{
		case 1:Login(); break;
		case 2:Register(); break;
		case 0:return 0;
		default:cout << "您的输入有误！请重新输入！" << endl;
			break;
		}
		cout << "\n\n\n请按回车键继续......" << endl;
		getchar();
		//system("cls");
	}
}