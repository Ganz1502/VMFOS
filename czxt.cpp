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

#define USER_NUM 9//���9���û�
#define SUM_MEM_SIZE 3*10*1024*1024//�ܴ��̿ռ�Ϊ30M
#define MEM_SIZE 3*1024*1024//ÿ���û����̿ռ�Ϊ3M
#define DISK_SIZE 1024//���̿��С1K
#define DISK_NUM 3*1024//���̿�3K
#define MSD 7//�����Ŀ¼��max subdirect
#define FATSIZE DISK_NUM*sizeof(fatItem)//FAT���С sizeof��һ���ڴ��������������������Ƿ���һ�������������͵Ĵ�С
#define USER_ROOT_STARTBLOCK FATSIZE/DISK_SIZE+1//�û���Ŀ¼��ʼ�̿��
#define USER_ROOT_SIZE sizeof(direct)//�û���Ŀ¼��С
#define MAX_FILE_SIZE 5000//����ļ�����

/*�ļ����ƿ�*/
typedef struct FCB {
	char fileName[20];//�ļ���Ŀ¼��
	int type;// 0--�ļ�  1--Ŀ¼ 
	char* content = NULL;//�ļ�����
	int size;//��С
	int firstDisk;//��ʼ�̿�
	int next;//��Ŀ¼��ʼ�̿��
	int sign;//0--��ͨĿ¼  1--��Ŀ¼
}FCB;

/*Ŀ¼*/
struct direct {
	FCB directItem[MSD + 2];
};

/*�ļ������*/
typedef struct fatItem {
	int item;//����ļ���һ�����̵�ָ��
	int state;//0-- ����  1--��ռ��
}fatItem, * myFAT;

/*�û���*/
typedef struct User {
	char name[20];//�û���
	char password[20];//�û�����
	int flag = 0;//������Ƿ񱻳�ʼ����
}person;
person* user;

/*���ļ���*/
struct table
{
	char fname[20];//�ļ���
	int cnt;//�򿪵Ĵ���
	int firdisk;//��ʾ�̿��
	int read = 0;//��Ȩ�ޣ�ӵ�ж�Ȩ�޺󣬿��Զ�
	int write = 0;//дȨ�ޣ�ӵ��дȨ�޺󣬿���׷�Ӻ���д
}Table[FATSIZE];

int tnum = 0;//���ڴ򿪵��ļ��ĸ���

string userName, userPassword;//��ǰ�û���������
int now_usernum = 0;//�����м����Ѿ�ע����û�
int now_user = 0;//����������û��ǵڼ����û�
myFAT fat;//�ҵ�myFAT��
direct* root;//��ǰ�û���Ŀ¼
direct* curDir;//��ǰ�û���ǰĿ¼
string curPath;//��ǰ�û���ǰ·��
char* fdisk;//���������ʼ��ַ
int flag_authority = 0;//Ȩ�ޣ���Щ�ļ��޷�open�ͱ�����ʣ���Ҫ���


/*�����ǰ�û�����·��*/
void Shell()
{
	//�����û�����·����ǰ��ɫ�ͱ���ɫ
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
	cout << curPath << ">";//����û�����·��
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

}
/*�����������*/
int save()
{
	ofstream fp;
	fp.open("os.dat", ios::binary);
	fp.write(fdisk, SUM_MEM_SIZE * sizeof(char));
	fp.close();
	return 1;
}
/*�û����̳�ʼ��*/
void startsys() //�鿴�Ƿ񴴽�������̣���ʼ�� 
{
	char* buf; //������ 
	user = (User*)(fdisk);//user�ӵ�һ���鿪ʼ��
	fdisk = (char*)malloc(SUM_MEM_SIZE);
	memset(fdisk, 0, SUM_MEM_SIZE);
	fat = (fatItem*)(fdisk + MEM_SIZE);//ǰ3M���û�
	ifstream fpr("os.dat", ios::binary);
	if (fpr)
	{
		fpr.read(fdisk, SUM_MEM_SIZE * sizeof(char));
		user = (User*)(fdisk);
		fpr.close();
	}
	else //����������� 
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
	//size of����һ���������������ռ���ڴ��ֽ�����
	//��ʼ��FAT��
	fat = (myFAT)(fdisk + DISK_SIZE + MEM_SIZE + now_user * MEM_SIZE);//fat��ַ
	fat[0].item = -1;
	fat[0].state = 1;//��ʼ��FAT��
	for (int i = 1; i < USER_ROOT_STARTBLOCK - 1; i++)
	{
		fat[i].item = i + 1;
		fat[i].state = 1;
	}
	//���û���Ŀ¼��ʾ�̺ŷָ�
	fat[USER_ROOT_STARTBLOCK].item = -1;
	fat[USER_ROOT_STARTBLOCK].state = 1;//��Ŀ¼���̿��
	//fat�������ʼ�������ǿ��е�
	for (int i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++)
	{
		fat[i].item = -1;
		fat[i].state = 0;
	}
	//��Ŀ¼��ʼ��
	root = (struct direct*)(fdisk + DISK_SIZE + FATSIZE + now_user * MEM_SIZE + MEM_SIZE);//��Ŀ¼��ַ
	root->directItem[0].sign = 1;
	root->directItem[0].firstDisk = USER_ROOT_STARTBLOCK + now_user * DISK_NUM + DISK_NUM;
	strcpy(root->directItem[0].fileName, ".");
	root->directItem[0].next = root->directItem[0].firstDisk;
	root->directItem[0].type = 1;
	root->directItem[0].size = USER_ROOT_SIZE;
	//��Ŀ¼��ʼ��
	root->directItem[1].sign = 1;
	root->directItem[1].firstDisk = USER_ROOT_STARTBLOCK + now_user * DISK_NUM + DISK_NUM;
	strcpy(root->directItem[1].fileName, "..");
	root->directItem[1].next = root->directItem[0].firstDisk;
	//cout << fat[25].item << " " << fat[26].item << " " << fat[27].item << endl;
	root->directItem[1].type = 1;
	root->directItem[1].size = USER_ROOT_SIZE;
	//��Ŀ¼��ʼ��
	for (int i = 2; i < MSD + 2; i++) {
		root->directItem[i].sign = 0;
		root->directItem[i].firstDisk = -1;
		strcpy(root->directItem[i].fileName, "");
		root->directItem[i].next = -1;
		root->directItem[i].type = 0;
		root->directItem[i].size = 0;
	}
	if (!save())
		cout << "��ʼ��ʧ�ܣ�" << endl;
}
/*��ȡ�����������ݡ�����������ֻ�������û����������¼��*/
void readDisk()
{
	fdisk = new char[SUM_MEM_SIZE];//������̴�С������
	ifstream fp;
	fp.open("os.dat", ios::in | ios::binary);//�Զ��ķ�ʽ��
	if (fp)
		fp.read(fdisk, SUM_MEM_SIZE);
	else
		cout << "��ʧ�ܣ�" << endl;
	fp.close();
	user = (User*)(fdisk);//���û��û���������
}
/*��ȡ�û������������ݡ�����������������Ŀǰ�ҵ�¼���û�ռ�Ĵ��̣���ȡ������Ϣ*/
void readUserDisk()
{
	fdisk = new char[SUM_MEM_SIZE];//������̴�С������
	ifstream fp;
	fp.open("os.dat", ios::in | ios::binary);//�Զ��ķ�ʽ��
	if (fp)
		fp.read(fdisk, SUM_MEM_SIZE);
	else
		cout << "��ʧ�ܣ�" << endl;
	fp.close();
	fat = (myFAT)(fdisk + DISK_SIZE + now_user * MEM_SIZE + MEM_SIZE);//fat��ַ
	user = (User*)(fdisk);//���û��û���������
	root = (struct direct*)(fdisk + DISK_SIZE + FATSIZE + now_user * MEM_SIZE + MEM_SIZE);//��Ŀ¼��ַ
	curDir = root;
	curPath = userName + ":\\";
}
/*�ͷŻ������ռ�*/
void release()
{
	delete fdisk;
}
/*���ļ�*/
void open(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//����Ŀ¼���Ƿ���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2)
	{
		cout << "�Ҳ������ļ���" << endl;
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
	cout << "��ѡ�����򿪸��ļ���Ȩ�ޣ�" << endl;
	cout << "1.ֻ��          2.ֻд         3.��д" << endl << endl;
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
			//cout << "Ȩ�ޣ�" << Table[tnum].write << Table[tnum].read<< endl;
			//cout << tnum << endl;
			break;
		}
		else
		{
			cout << "�������������������룺" << endl;
			cin >> a;
		}
	}
	cout << fName << "�Ѿ��򿪣�" << endl;
	if (flag == 0)
		tnum++;
	cout << "���ڴ򿪵��ļ�����Ŀ�ǣ�" << tnum << endl;
	getchar();
}
void close(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//����Ŀ¼���Ƿ���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2)
	{
		cout << "�Ҳ������ļ���" << endl;
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
		cout << "���ļ�δ���򿪹���" << endl;
	}
	else cout << "���ļ��ѹرգ�" << endl;
	cout << "������Ȼ�򿪵��ļ�����Ŀ�ǣ�" << tnum << endl;
}
void write(string* str, char* content, int size)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//�ڵ�ǰĿ¼�²���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			//������ͬ �������ļ� �����ҵ���
			break;
	if (i >= MSD + 2)
	{
		cout << "�Ҳ������ļ���" << endl;
		return;
	}
	strcpy(fName, str[1].c_str());
	//����Ŀ¼���Ƿ���Ŀ���ļ�
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2)
	{
		cout << "�Ҳ������ļ���" << endl;
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
			cout << "���ļ���û��open���޷�������������" << endl;
		}
		else if (write == 0)
		{
			cout << "���ļ�û��д��Ȩ�ޣ�" << endl;
		}
		else
			flag_authority = 1;
	}
	if(flag_authority ==1)
	{
		flag_authority = 0;
		int cur = i;//��ǰĿ¼����±�
		int fSize = curDir->directItem[cur].size;//Ŀ���ļ���С
		int item = curDir->directItem[cur].firstDisk;//Ŀ���ļ�����ʼ���̿��
		int item2 = item - (now_user + 1) * DISK_NUM;
		while (fat[item2].item != -1)
			item = fat[item2].item;//���㱣����ļ������һ���̿��*/
		char* first = fdisk + item * DISK_SIZE + fSize % DISK_SIZE;//������ļ���ĩ��ַ
		//����̿�ʣ�ಿ�ֹ�д����ֱ��д��ʣ�ಿ��
		if (DISK_SIZE - fSize % DISK_SIZE > size)
		{
			strcpy(first, content);
			curDir->directItem[cur].size += size;
		}
		//����̿�ʣ�ಿ�ֲ���д�����ҵ����д��̿�д��
		else
		{
			//�Ƚ���ʼ����ʣ�ಿ��д��
			for (j = 0; j < DISK_SIZE - fSize % DISK_SIZE; j++)
			{
				first[j] = content[j];
			}

			int res_size = size - (DISK_SIZE - fSize % DISK_SIZE);//ʣ��Ҫд�����ݴ�С
			int needDisk = res_size / DISK_SIZE;//ռ�ݵĴ��̿�����
			int needRes = res_size % DISK_SIZE;//ռ�����һ����̿�Ĵ�С

			if (needRes > 0)
				needDisk += 1;
			for (j = 0; j < needDisk; j++)
			{
				for (i = USER_ROOT_STARTBLOCK + 1; i < DISK_NUM; i++)
					if (fat[i].state == 0)
						break;
				if (i >= DISK_NUM)
				{
					cout << "�����ѱ������꣡" << endl;
					return;
				}
				first = fdisk + i * DISK_SIZE + (now_user + 1) * DISK_NUM;//���д�����ʼ�������ַ
				//��д�����һ����̣���ֻдʣ�ಿ������
				if (j == needDisk - 1)
				{
					for (int k = 0; k < size - (DISK_SIZE - fSize % DISK_SIZE - j * DISK_SIZE); k++)
						first[k] = content[k];
				}
				else {
					for (int k = 0; k < DISK_SIZE; k++)
						first[k] = content[k];
				}
				//�޸��ļ����������
				fat[item2].item = i;
				fat[i].state = 1;
				fat[i].item = -1;
			}
			curDir->directItem[cur].size += size;
		}
	}
}
/*touch�����ļ�*/
int touch(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//����ļ����Ƿ�Ϸ�
	if (!strcmp(fName, ""))return 0;
	if (str[1].length() > 20) {
		cout << "�ļ���̫�������������룡" << endl;
		return 0;
	}
	//�ҵ�ǰĿ¼���Ƿ����ļ�����
	for (int pos = 2; pos < MSD + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0) {
			cout << "��ǰĿ¼���Ѵ����ļ�����!" << endl;
			return 0;
		}
	}
	//��鵱ǰĿ¼�¿ռ��Ƿ�����
	int i;
	for (i = 2; i < MSD + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= MSD + 2) {
		cout << "��ǰĿ¼�¿ռ�����" << endl;
		return 0;
	}
	//����Ƿ��п��д��̿�
	int j;
	for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= DISK_NUM) {
		cout << "�޿����̿飡" << endl;
		return 0;
	}
	//�ļ���ʼ��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j + now_user * DISK_NUM + DISK_NUM;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].next = j + now_user * DISK_NUM + DISK_NUM;
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	return 1;
}
/*��ʾ��ǰĿ¼����Ŀ¼���ļ���Ϣ*/
void dir()
{
	cout << fixed << left << setw(20) << "�ļ���" << setw(10) << "����" << setw(10) << "��С</B>" << setw(10) << "��ʼ���̿��" << endl;
	for (int i = 2; i < MSD + 2; i++) {
		if (curDir->directItem[i].firstDisk != -1)
		{
			cout << fixed << setw(20) << curDir->directItem[i].fileName;
			if (curDir->directItem[i].type == 0) {//��ʾ�ļ�
				cout << fixed << setw(10) << " " << setw(10) << curDir->directItem[i].size;
			}
			else {//��ʾĿ¼
				cout << fixed << setw(10) << "<DIR>" << setw(10) << " ";
			}
			cout << setw(10) << curDir->directItem[i].firstDisk << endl;
		}
	}
}
/*����Ŀ¼*/
int mkdir(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//���Ŀ¼���Ƿ�Ϸ�
	if (!strcmp(fName, ""))return 0;
	if (str[1].length() > 10) {
		cout << "Ŀ¼��̫�������������룡" << endl;
		return 0;
	}
	if (!strcmp(fName, ".") || !strcmp(fName, "..")) {
		cout << "Ŀ¼���Ʋ��Ϸ���" << endl;
		return 0;
	}
	//�ҵ�ǰĿ¼���Ƿ���Ŀ¼����
	for (int pos = 2; pos < MSD + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 1) {
			cout << "��ǰĿ¼���Ѵ���Ŀ¼����!" << endl;
			return 0;
		}
	}
	//��鵱ǰĿ¼�¿ռ��Ƿ�����
	int i;
	for (i = 2; i < MSD + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= MSD + 2) {
		cout << "��ǰĿ¼�¿ռ�����" << endl;
		return 0;
	}
	//����Ƿ��п��д��̿�
	int j;
	for (j = USER_ROOT_STARTBLOCK + 1; j < DISK_NUM; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= DISK_NUM) {
		cout << "�޿����̿飡" << endl;
		return 0;
	}
	//����Ŀ¼��ʼ��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j + now_user * DISK_NUM + DISK_NUM;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].next = j + now_user * DISK_NUM + DISK_NUM;
	curDir->directItem[i].type = 1;
	curDir->directItem[i].size = USER_ROOT_SIZE;
	direct* cur_mkdir = (direct*)(fdisk + curDir->directItem[i].firstDisk * DISK_SIZE);//����Ŀ¼�������ַ
	//ָ��ǰĿ¼��Ŀ¼��
	cur_mkdir->directItem[0].sign = 0;
	cur_mkdir->directItem[0].firstDisk = curDir->directItem[i].firstDisk;
	strcpy(cur_mkdir->directItem[0].fileName, ".");
	cur_mkdir->directItem[0].next = cur_mkdir->directItem[0].firstDisk;
	cur_mkdir->directItem[0].type = 1;
	cur_mkdir->directItem[0].size = USER_ROOT_SIZE;
	//ָ����һ��Ŀ¼��Ŀ¼��
	cur_mkdir->directItem[1].sign = curDir->directItem[0].sign;
	cur_mkdir->directItem[1].firstDisk = curDir->directItem[0].firstDisk;
	strcpy(cur_mkdir->directItem[1].fileName, "..");
	cur_mkdir->directItem[1].next = cur_mkdir->directItem[1].firstDisk;
	cur_mkdir->directItem[1].type = 1;
	cur_mkdir->directItem[1].size = USER_ROOT_SIZE;
	//��Ŀ¼��ʼ��
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
/*ɾ��Ŀ¼*/
int rmdir(string* str)
{
	char fName[20]; strcpy(fName, str[1].c_str());
	//�ڵ�ǰĿ¼�²�ѯĿ��Ŀ¼
	int i, j; direct* tempDir = curDir;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(tempDir->directItem[i].fileName, fName) && tempDir->directItem[i].type == 1)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ�����Ŀ¼��" << endl;
		return 0;
	}
	//����Ƿ�����Ŀ¼���ļ�
	tempDir = (direct*)(fdisk + tempDir->directItem[i].next * DISK_SIZE);
	for (j = 2; j < MSD + 2; j++) {
		if (tempDir->directItem[j].next != -1) {
			cout << "ɾ��ʧ�ܣ���Ŀ¼������Ŀ¼���ļ�!" << endl;
			return 0;
		}
	}
	int item = curDir->directItem[i].firstDisk;//Ŀ���ļ�����ʼ���̿��
	int item2 = item - (now_user + 1) * DISK_NUM;
	fat[item2].state = 0;//�޸��ļ������
	//�޸�Ŀ¼��

	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].next = -1;
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	return 1;
}
/*ɾ���ļ�*/
int del(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//�ڸ�Ŀ¼����Ŀ���ļ�
	int i, temp;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ������ļ���" << endl;
		return 0;
	}

	int item = curDir->directItem[i].firstDisk;//Ŀ���ļ�����ʼ���̿��
	int item2 = item - (now_user + 1) * DISK_NUM;
	//�����ļ����������ɾ���ļ���ռ�ݵĴ��̿�
	while (item2 != -1)
	{
		temp = fat[item2].item;
		fat[item2].item = -1;
		fat[item2].state = 0;
		item2 = temp;
	}
	//�ͷ�Ŀ¼��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].next = -1;
	curDir->directItem[i].type = 0;
	//	curDir->directItem[i].content=NULL;
	//	cout << curDir->directItem[i].content<< endl;
	curDir->directItem[i].size = 0;
	//cout << "����ɾ���Ƿ�ִ��" << endl;
	return 1;
}
/*�л�Ŀ¼*/
void cd(string* str)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	string objPath = str[1];//Ŀ��·��
	if (objPath[objPath.size() - 1] != '\\')objPath += "\\";
	int start = -1;
	int end = 0;//��\Ϊ�ָÿ����ʼ�±�ͽ����±�
	int i, j, len = objPath.length();
	direct* tempDir = curDir;
	string oldPath = curPath;//�����л�ǰ��·��
	string temp;
	for (i = 0; i < len; i++) {
		//���Ŀ��·����\��ͷ����Ӹ�Ŀ¼��ʼ��ѯ
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
			//���Ŀ¼�Ƿ����
			for (j = 0; j < MSD + 2; j++)
				if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
					break;
			if (j >= MSD + 2) {
				curPath = oldPath;
				cout << "�Ҳ�����Ŀ¼��" << endl;
				return;
			}
			//���Ŀ��·��Ϊ".."���򷵻���һ��
			if (temp == "..") {
				if (tempDir->directItem[j - 1].sign != 1) {
					int pos = curPath.rfind('\\', curPath.length() - 2);
					curPath = curPath.substr(0, pos + 1);
				}
			}
			//���Ŀ��·��Ϊ"."����ָ��ǰĿ¼
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
//��ʾ�ļ�����
int more(string* str, char* buf)
{
	char fName[20];
	strcpy(fName, str[1].c_str());
	//����Ŀ¼���Ƿ���Ŀ���ļ�
	int i, j;
	for (i = 2; i < MSD + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= MSD + 2) {
		cout << "�Ҳ������ļ���" << endl;
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
			cout << "���ļ���û��open���޷�������������" << endl;
			return 0;
		}
		else if (read2 == 0)
		{
			cout << "���ļ�û�ж���Ȩ�ޣ�" << endl;
			return 0;
		}
		else
			flag_authority = 1;
	}
	if(flag_authority ==1)
	{
		flag_authority = 0;
		int item = curDir->directItem[i].firstDisk;//Ŀ���ļ�����ʼ���̿��
		//cout << "���� Ŀ���ļ��̺�" << curDir->directItem[i].firstDisk << endl;
		int fSize = curDir->directItem[i].size;//Ŀ���ļ��Ĵ�С
		//cout << "���� Ŀ���ļ���С" << fSize << endl;
		int needDisk = fSize / DISK_SIZE;//ռ�ݵĴ��̿�����
		//cout << "���� Ŀ���ļ����̿�����" << needDisk << endl;
		int needRes = fSize % DISK_SIZE;//ռ�����һ����̿�Ĵ�С
		//cout << "���� Ŀ���ļ����һ����̺Ŵ�С" << needRes << endl;
		if (needRes > 0)
			needDisk += 1;
		char* first = fdisk + item * DISK_SIZE;//��ʼ���̿�������ַ
		int item2 = item - (now_user + 1) * DISK_NUM;
		//��Ŀ¼�ļ����ݿ�������������
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
			if (i != needDisk - 1)//û�ж������һ�����ʱ������һ��Ҫ���Ŀ�
			{
				item = fat[item2].item;
				first = fdisk + item * DISK_SIZE;
			}
		}
		return 1;
	}
}//
/*�����ļ����Ŀ¼�Ӻ���*/
void xcopy_fun(direct* sourceDir, direct* objDir)
{
	//������Ŀ¼���ļ�
	int i;
	for (int k = 2; k < MSD + 2; k++) {
		//�����Ŀ¼������Ŀ¼���ظ����ñ�����
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
		//������ļ��������ļ���Ŀ·����
		else {
			string newStr[3]; newStr[0] = "touch"; newStr[1] = sourceDir->directItem[k].fileName;
			if (newStr[1] == "")continue;
			//���ļ�
			char* content = new char[MAX_FILE_SIZE];//�ļ�����
			curDir = sourceDir;
			more(newStr, content);
			curDir = objDir;
			//�����ļ�������
			if (touch(newStr))
				write(newStr, content, strlen(content));
			delete[] content;
		}
	}
}
/*�����ļ����Ŀ¼*/
void copy(string* str)
{
	char source[20]; 
	strcpy(source, str[1].c_str()); string s_source = str[1];//ԭ�ļ���Ŀ¼
	char obj[20];
	strcpy(obj, str[2].c_str()); string s_obj = str[2];//Ŀ���ļ���Ŀ¼
	//���ԭ�ļ���Ŀ¼�Ƿ���ڲ��Ϸ�
	if (s_source == "" || s_obj == "") {
		cout << "�������" << endl;
		return;
	}
	int i, j;
	for (i = 2; i < MSD + 2; i++) {
		if (!strcmp(curDir->directItem[i].fileName, source))
			break;
	}
	if (i >= MSD + 2) {
		cout << "�Ҳ���Դ�ļ���" << endl;
		return;
	}
	direct* cur = curDir;//���浱Ŀ¼
	string oldPath = curPath;//���浱ǰ·��
	string newStr[3];
	newStr[0] = "touch";
	char* content = new char[MAX_FILE_SIZE];
	//���ԭ�ļ�����Ϊ"�ļ�"
	flag_authority = 1;
	if (curDir->directItem[i].type == 0)
	{
		//��ȡԴ�ļ�����
		more(str, content);
	}
	//����Ŀ���ļ���Ŀ¼·���л���ǰĿ¼
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
	//���ԭ�ļ�����Ϊ"�ļ�"
	if (cur->directItem[i].type == 0) {
		if (touch(newStr))
		{
			write(newStr, content, strlen(content));
			cout << "���Ƴɹ�!" << endl;
		}
		else
			cout << "����ʧ��!" << endl;
	}
	//���ԭ�ļ�����Ϊ"Ŀ¼"
	if (cur->directItem[i].type == 1) {
		direct* tempDir = (direct*)(fdisk + cur->directItem[i].next * DISK_SIZE);
		for (j = 2; j < MSD + 2; j++) {
			if (tempDir->directItem[j].next != -1) {
				cout << "��Ŀ¼����Ŀ¼���ļ���" << endl;
				break;
			}
		}
		mkdir(newStr);
	}
	curDir = cur;
	curPath = oldPath;
	delete[] content;
}
/*������Ŀ¼���ļ�*/
void xcopy(string* str)
{
	char source[20]; strcpy(source, str[1].c_str()); string s_source = str[1];//ԭ�ļ���Ŀ¼
	char obj[20]; strcpy(obj, str[2].c_str()); string s_obj = str[2];//Ŀ���ļ���Ŀ¼
	string oldPath = curPath;
	flag_authority = 1;
	//���ԭ�ļ���Ŀ¼�Ƿ���ڲ��Ϸ�
	if (s_source == "" || s_obj == "") {
		cout << "�������" << endl;
		return;
	}
	int i;
	for (i = 2; i < MSD + 2; i++) {
		if (!strcmp(curDir->directItem[i].fileName, source))
			break;
	}
	if (i >= MSD + 2) {
		cout << "�Ҳ���ԭ�ļ���" << endl;
		return;
	}
	direct* cur = curDir;//���浱ǰ·��
	//�����ļ�
	if (cur->directItem[i].type == 0)
	{
		copy(str);
	}
	//����Ŀ¼
	else
	{
		//����Ŀ���ļ���Ŀ¼·���л���ǰĿ¼
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
		//����xcopy�Ӻ�����������Ŀ¼�����ļ�
		direct* sourceDir = (direct*)(fdisk + cur->directItem[i].next * DISK_SIZE);//ԴĿ¼
		if (mkdir(newStr)) {
			for (i = 2; i < MSD + 2; i++) {
				if (!strcmp(curDir->directItem[i].fileName, newStr[1].c_str()))
					break;
			}
			direct* objDir = (direct*)(fdisk + curDir->directItem[i].next * DISK_SIZE);//Ŀ��Ŀ¼
			xcopy_fun(sourceDir, objDir);
		}
	}
	curDir = cur;
	curPath = oldPath;
}
/*���ҵ�ǰĿ¼����Ŀ�����ݵ��ļ�*/
void find(char* obj, int& flag)
{
	for (int i = 2; i < MSD + 2; i++) {
		//�������Ч�ļ��������Ƿ���Ŀ������
		if (curDir->directItem[i].firstDisk != -1 && curDir->directItem[i].type == 0 && curDir->directItem[i].size != 0 && strcmp(curDir->directItem[i].fileName, "") != 0) {
			char* buf = new char[MAX_FILE_SIZE];
			string newStr[2]; newStr[0] = "more"; newStr[1] = curDir->directItem[i].fileName;
			more(newStr, buf);
			if (strstr(buf, obj) != NULL) {
				cout << "# " << curPath << newStr[1] << "����Ŀ�����ݣ�" << endl;
				flag = 1;
			}
			delete[] buf;
		}
		//�����Ŀ¼����ݹ������Ŀ¼���ļ�����
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
/*���뱾���ļ�������*/
void import(string* str)
{
	flag_authority = 1;
	string filePath = str[1];
	char* content = new char[MAX_FILE_SIZE];//���û�����
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
		if (touch(newStr))//��ȡ�����ļ����ݲ������ļ�д��
			write(newStr, content, strlen(content));
		delete[] content;
		cout << "�����ļ��ɹ�" << endl;
	}
	else {
		cout << "�򿪱����ļ�ʧ�ܣ�" << endl;
	}
}
/*�����������ļ�������*/
void Export(string* str)
{
	flag_authority = 1;
	string source = str[1], filePath = str[2];
	char* content = new char[MAX_FILE_SIZE];//���û�����
	more(str, content);//��ȡ�ļ�����
	fstream fp;
	fp.open(filePath, ios::out);
	if (fp) {
		fp << content;
		cout << "�����ļ��ɹ���" << endl;
	}
	else {
		cout << "�����ļ�ʧ��" << endl;
	}
	delete[] content;
}
/*��д�ļ�����*/
void rewrite(string* str)
{
	if (del(str) && touch(str)) {
		//��鵱ǰĿ¼���Ƿ���Ŀ���ļ�
		int i;
		for (i = 2; i < MSD + 2; i++)
			if (!strcmp(curDir->directItem[i].fileName, str[1].c_str()) && curDir->directItem[i].type == 0)
				break;
		if (i >= MSD + 2)
		{
			cout << "�Ҳ������ļ���" << endl;
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
				cout << "���ļ���û��open���޷�������������" << endl;
			}
			else if (write2 == 0)
			{
				cout << "���ļ�û��д��Ȩ�ޣ�" << endl;
			}
			else
				flag_authority = 1;
		}
		if(flag_authority == 1)
		{
			//����д�����ݣ�����#�Ž���
			char ch;
			char content[MAX_FILE_SIZE];//���û�����
			int size = 0;
			cout << "�������ļ����ݣ�����'#'Ϊ������־" << endl;
			while (1)
			{
				ch = getchar();
				if (ch == '#')break;
				if (size >= MAX_FILE_SIZE)
				{
					cout << "�����ļ����ݹ�����" << endl;
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

/*������*/
void help()
{
	cout << fixed << left;
	cout << "********************  ����  ********************" << endl << endl;
	cout << setw(40) << "cd ·��" << setw(10) << "�л�Ŀ¼" << endl;
	cout << setw(40) << "create �ļ���" << setw(10) << "�����ļ�" << endl;
	cout << setw(40) << "delete �ļ���" << setw(10) << "ɾ���ļ�" << endl;
	cout << setw(40) << "write �ļ���" << setw(10) << "д�����ݣ�׷��ģʽ������ģʽ��" << endl;
	cout << setw(40) << "mkdir Ŀ¼��" << setw(10) << "����Ŀ¼" << endl;
	cout << setw(40) << "rmdir Ŀ¼��" << setw(10) << "ɾ��Ŀ¼" << endl;
	cout << setw(40) << "dir" << setw(10) << "��ʾ��ǰĿ¼��������Ŀ¼�����ļ�" << endl;
	cout << setw(40) << "copy Դ�ļ� Ŀ���ļ�" << setw(10) << "�����ļ�" << endl;
	cout << setw(40) << "xcopy ԴĿ¼ Ŀ��Ŀ¼" << setw(10) << "����Ŀ¼" << endl;
	cout << setw(40) << "open" << setw(10) << "���ļ�" << endl;
	cout << setw(40) << "close" << setw(10) << "�ر��ļ�" << endl;
	cout << setw(40) << "head  -num" << setw(10) << "��ʾ�ļ���ǰnum��" << endl;
	cout << setw(40) << "tail  -num" << setw(10) << "��ʾ�ļ�β���ϵ�num��" << endl;
	cout << setw(40) << "import �����ļ�·��" << setw(10) << "�����ļ�" << endl;
	cout << setw(40) << "export �ļ��� �����ļ�·��" << setw(10) << "�����ļ�" << endl;
	cout << setw(40) << "read �ļ���" << setw(10) << "��ʾ�ļ�����" << endl;
	cout << setw(40) << "save" << setw(10) << "�����������" << endl;
	cout << setw(40) << "help" << setw(10) << "�����ĵ�" << endl;
	cout << setw(40) << "clear" << setw(10) << "����" << endl;
	cout << setw(40) << "exit" << setw(10) << "�˳�" << endl << endl;
}
/*�˵���*/
void menu()
{
	//system("cls");
	getchar();
	cout << "***************** Hello  " << userName << "! *****************" << endl;
	readUserDisk();//��ȡ�������û���λ��
	//cout<<"����init" <<user[now_user].flag <<endl;
	if (user[now_user].flag == 0)//���û�г�ʼ��
	{
		init();//�û��ĳ�ʼ��
		user[now_user].flag = 1;//���Ϊ��ʼ����
		save();//����
	}
	while (true)
	{
		Shell();
		//�Կո�Ϊ���ߣ���ȡ����,����stringstream���ָ�����
		string cmd;
		getline(cin, cmd);
		stringstream stream;
		stream.str(cmd);
		string str[5];
		for (int i = 0; stream >> str[i]; i++);
		//����˵�ѡ��
		if (str[0] == "exit") {
			exit(0);
		}
		else if (str[0] == "create") {
			if (touch(str))
				cout << "�ļ������ɹ���" << endl;
		}
		else if (str[0] == "dir") {
			dir();
		}
		else if (str[0] == "mkdir") {
			if (mkdir(str))
				cout << str[1] << "�����ɹ���" << endl;
		}
		else if (str[0] == "open") {
			open(str);
		}
		else if (str[0] == "close") {
			close(str);
		}
		else if (str[0] == "read")
		{
			char* buf = new char[MAX_FILE_SIZE];//���û�����
			if (more(str, buf) == 0)
			{
				cout << "*************************************" << endl;
				cout << "�ܱ�Ǹ����ȡʧ�ܣ�" << endl;
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
				cout << str[1] << "ɾ���ɹ���" << endl;
		}
		else if (str[0] == "rmdir") {
			if (rmdir(str))
				cout << str[1] << "ɾ���ɹ���" << endl;
		}
		else if (str[0] == "write") {
			int choice;
			cout << "***********************ģʽѡ��**********************" << endl;
			cout << "1.׷��ģʽ                   2.����ģʽ               " << endl << endl;
			cout << "----------------------------------------------------" << endl;
			cout << "����������ѡ��" << endl;
			cout << "****************************************************" << endl << endl;
			cin >> choice;
			getchar();
			if (choice == 1)
			{
				//��鵱ǰĿ¼���Ƿ���Ŀ���ļ�
				int i;
				for (i = 2; i < MSD + 2; i++)
					if (!strcmp(curDir->directItem[i].fileName, str[1].c_str()) && curDir->directItem[i].type == 0)
						break;
				if (i >= MSD + 2) {
					cout << "�Ҳ������ļ���" << endl;
					continue;
				}
				//����д�����ݣ�����#�Ž���
				char ch;
				char content[MAX_FILE_SIZE];//���û�����
				int size = 0;//����ļ��Ĵ�С
				cout << "�������ļ����ݣ�����'#'Ϊ������־" << endl;
				while (1)
				{
					ch = getchar();
					if (ch == '#')
					{
						ch = '\0';
						break;
					}
					if (size >= MAX_FILE_SIZE) {
						cout << "�����ļ����ݹ�����" << endl;
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
				cout << "������������" << endl;
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
		else if (str[0] == "head") {//ǰnum��
			char* buf = new char[MAX_FILE_SIZE];//���û�����
			more(str, buf);
			if (more(str, buf) == 0)
			{
				cout << "*************************************" << endl;
				cout << "�ܱ�Ǹ����ȡʧ�ܣ�" << endl;
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
		else if (str[0] == "tail") {//��num��
			char* buf = new char[MAX_FILE_SIZE];//���û�����
			char* buf2 = new char[MAX_FILE_SIZE];//���û�����
			more(str, buf);
			if (more(str, buf) == 0)
			{
				cout << "*************************************" << endl;
				cout << "�ܱ�Ǹ����ȡʧ�ܣ�" << endl;
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
				//cout << "����" << (num1 - n) << endl;
				cout << buf2 << endl;
				cout << "*************************************" << endl;
				delete[] buf;
			}
		}
		else if (str[0] == "save") {
			if (save())
				cout << "����ɹ���" << endl;
			else
				cout << "����ʧ��" << endl;
		}
		else if (str[0] == "clear") {
			system("cls");//����
		}
		else if (str[0] == "help") {
			help();//�����ĵ�
		}
		else {
			cout << "�Ҳ������������������" << endl;
		}
	}
	release();
}
/*�û�ע�Ṧ��*/
void Register()
{
	string input;
	int f = 0;
	char name[20], word[20];
	int i = 0;
	cout << "�û�����";
	cin >> name;
	cout << "���룺";
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
		cout << "�û����Ѵ����ޣ�" << endl;
	}
	if (strcmp(user[i].name, "") == 0)
	{
		strcpy(user[i].name, name);
		strcpy(user[i].password, word);
		f = 1;
		//LOGIN = 1;
		cout << "ע��ɹ���" << endl;
		save();
	}
	else
		cout << "�û����Ѵ��ڣ�" << endl;
}
/*�û���¼����*/
void Login()
{
	string input;
	int f = 0;
	char name[20], word[20];
	cout << "�û�����";
	cin >> name;
	cout << "���룺";
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
			cout << "��½�ɹ���" << endl;
			userName = name;
			menu();
			break;
		}
		i++;
	}
	if (f == 0)
		cout << "�û������������" << endl;
}
/*����������ʼ���棩*/
int main()
{
	system("color 0B");
	while (true)
	{
		startsys();//�ܵĴ��� ����
		readDisk();//duqu
		cout << "****************��ӭ����cjb���ļ�ϵͳ****************" << endl;
		cout << "----------------------------------------------------" << endl;
		cout << "****************************************************" << endl << endl;
		cout << "1����¼               2��ע��              0���˳�" << endl << endl;
		cout << "****************************************************" << endl;
		cout << "�����룺";
		int choice;
		cin >> choice;
		switch (choice)
		{
		case 1:Login(); break;
		case 2:Register(); break;
		case 0:return 0;
		default:cout << "���������������������룡" << endl;
			break;
		}
		cout << "\n\n\n�밴�س�������......" << endl;
		getchar();
		//system("cls");
	}
}