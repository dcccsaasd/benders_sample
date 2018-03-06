#pragma once
#include <vector>
#include <ilcplex/ilocplex.h>
#include "type.h"
#include "Wait.h"
#include "Maxflow.h"
using namespace std;
//�޸İ棬ֱ��ʹ��Dual model
struct Info
{
	enum varType{theta_up_nk2t,theta_low_nk2t,theta_up_nik3t,theta_low_nik3t,phi_n1,phi_up_n2t,delta_nt,unknow};  //Ŀ�꺯��ӵ�еı���
	varType type;
	int k,n,i,t,tt;
	Info(varType type,int k,int n,int i,int t,int tt):type(type),k(k),n(n),i(i),t(t),tt(tt){}
	Info(varType type,int k,int n,int t,int tt):type(type),k(k),n(n),t(t),tt(tt),i(n){}
	Info(varType type,int k,int n,int t):type(type),k(k),n(n),t(t),tt(0),i(0){}
	Info(varType type,int n,int t):type(type),n(n),t(t),k(0),tt(0),i(0){}
	Info(varType type,int n):type(type),n(n),k(0),t(0),tt(0),i(0){}
	Info():type(varType::unknow),k(0),n(0),t(0),tt(0),i(0){}
};
class Model
{
public:
	Model(char *filename,char *outfile);
	Model();
	~Model();
	//һЩ���������ļ�����
	const int M ;    //���ʽ�� M��ֵ,�˴�����Ϊ 100000
	const double epsilon;
	const int M_test;
	int K,N,T;
	vector<double> a,b,B;
	vector<vector<double> > tau_low,tau_up,d_low,d_up,mu,cost;

	int subtourCutCount,fraInfbendersCount,fraOptbendersCount,intInfbendersCount,intOptbendersCount;
	//master ������ر���
	IloNumVar eta;
	IloIntVarMatrix y;   //  (N+1) * T 
	IloNumMatrix yy;     //  (N+1) * T
	IloIntVarMatrix3 z;  // (N+1) * (N+1) * T
	IloNumMatrix3 zz;    // (N+1) * (N+1) * T

	IloNumMatrix e;     // (N+1) * (N+1)
	IloNumVarArray q1;  //N
	IloNumVarMatrix alpha,x0,q0,s0_up,s0_low; //N * T
	IloNumVarMatrix4 s_up,s_low,l_up,l_low,h_up,h_low,x,q;  //N * N * T * T
	IloNumVarMatrix5 u_up,u_low,v_up,v_low;                 //K * N * N * T * T 

	//Dual subproblem ������ر���
	IloObjective dualObj;
	IloNumVarArray phi_n1;
	IloNumVarMatrix theta_up_n1t,theta_low_n1t,phi_up_n2t,phi_low_n2t,delta;
	IloNumVarMatrix3 theta_up_nk2t,theta_low_nk2t;
	IloNumVarMatrix4 phi_up_ni3t,phi_low_ni3t,lambda;
	IloNumVarMatrix5 theta_up_nik3t,theta_low_nik3t;


	RangeMatrix alphaRngs;  //��������ȡ aplpha��ֵ
	RangeMatrix q_nt_Rngs;  //��ȡ q_n_1 �� q0_n_t
	RangeMatrix4 q_nit_tt;
	IloEnv masterEnv,workerEnv;
	IloCplex masterCplex,workerCplex;
	IloModel masterModel,workerModel;

	vector<Info *> Infos;  //dual subproblem variable �����ǵ��±������
	IloNum runtime;

	Maxflow *f;
	Wait *wait;

	void createMasterILP();
	void createWorkerLP();
	void rebuildWorkerLP(const IloNumMatrix &yy);     //�ڵõ�master����Ľ�����¹���������
	void initWorkerVariable();  //��ʼ����������һЩ������ά��
	IloExpr buildOptimalBendersExpr(); //����benders cut
	IloExpr buildInfeasibleBendersExpr();

	//��ȡ�ļ�
	void readLine(char* chs,double *data,int n); //chsΪ�ַ���ʽ����,�Կո�ָ��\n ����\r\n��β����������ȡn�������浽data��
	inline void copyData(vector<double> &to,double * from,int beginIdx,int len);
	void readData(char *filename);  //��ȡ����
	bool isSkipLine(char *str);
	void getOneLineData(FILE *fp,char *line,double *data,int n);

	void readData2(char *filename);
	void print();
	void showResult(char *filename);
};
