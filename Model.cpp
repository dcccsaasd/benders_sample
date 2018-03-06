#include "Model.h"
#include "type.h"
#include "callback.h"
#include <cstdio>
#include <fstream>
using namespace std;

Model::Model():M(1e3),epsilon(1e-2),M_test(1e4){}
Model::Model(char *filename,char *outfile):M(1e3),epsilon(1e-4),M_test(1e4)
{
	subtourCutCount=fraInfbendersCount=fraOptbendersCount=intInfbendersCount=intOptbendersCount=0;
	f=new Maxflow();
	wait=new ConstantWaiting(1);

	readData2(filename);
	cout<<"finished read data"<<endl;
	//print();    //check whether read data correct or not
	time_t t1 = clock();
	createMasterILP();
	cout<<"finished create master problem"<<endl;
	createWorkerLP();
	cout<<"finished create worker problem"<<endl;
	//workerCplex.setParam(IloCplex::Param::Preprocessing::Presolve,IloFalse);
	workerCplex.setParam(IloCplex::Param::RootAlgorithm,IloCplex::Algorithm::Primal);
	workerCplex.setParam(IloCplex::NumericalEmphasis,1);
	workerCplex.setOut(workerEnv.getNullStream());
	masterCplex.setParam(IloCplex::Param::MIP::Strategy::Search,IloCplex::Traditional);
	masterCplex.setParam(IloCplex::Param::Preprocessing::Presolve,IloFalse);
	masterCplex.setParam(IloCplex::NumericalEmphasis,0);
	masterCplex.setParam(IloCplex::TiLim,7200);

	//masterCplex.use(myuserCut(masterEnv,*this));
	masterCplex.use(mylazyCut(masterEnv,*this));

	//masterCplex.use(myuserCut_0_node(masterEnv,*this));
	//masterCplex.use(mylazyCut_0_node(masterEnv,*this));
	masterCplex.use(myuserCut_tour(masterEnv,*this,2));
	
	masterCplex.solve();
	time_t t2=clock();
	runtime=(t2-t1)*1.0/1000;
	if(masterCplex.getStatus()==IloAlgorithm::Infeasible){
		cout<<"Infeasible"<<endl;
	}else if(masterCplex.getStatus()==IloAlgorithm::Unbounded){
		cout<<"Unbounded"<<endl;
	}else if(masterCplex.getStatus()==IloAlgorithm::Unknown){
		cout<<"Unknown"<<endl;
	}else{
		showResult(outfile);
	}
	for(int i=0;i<Infos.size();i++)
		delete Infos[i];
	workerCplex.end();
	masterCplex.end();
	workerEnv.end();
	masterEnv.end();
}

Model::~Model(){}

//����������ģ�ͣ�ȱ��23ʽ����������ͼ
void Model::createMasterILP()
{
	int i,j,n,t;
	char name[20];
	masterModel=IloModel(masterEnv);
	masterCplex=IloCplex(masterEnv);
	z=IloIntVarMatrix3(masterEnv, N + 1);
	zz=IloNumMatrix3(masterEnv, N + 1);
	y=IloIntVarMatrix(masterEnv,N+1);
	yy=IloNumMatrix(masterEnv,N+1);
	eta=IloNumVar(masterEnv);
	eta.setName("eta");
	for(i = 0; i <= N; i++) {
		y[i]=IloIntVarArray(masterEnv,T+1,0,1);
		yy[i]=IloNumArray(masterEnv,T+1);
		for(t=1;t<=T;t++)
		{
			sprintf(name,"y_%d_%d",i,t);
			y[i][t].setName(name);
		}

		z[i] = IloIntVarMatrix(masterEnv, N + 1);
		zz[i] = IloNumMatrix(masterEnv, N + 1);
		for(j = 0; j <= N; j++) {
			if(i==0||j==0){
				z[i][j] = IloIntVarArray(masterEnv, T + 1, 0, 2);
			}else{
				z[i][j] = IloIntVarArray(masterEnv, T + 1, 0, 1);
			}
			zz[i][j] = IloNumArray(masterEnv, T + 1);
			for(t=1;t<=T;t++)
			{
				sprintf(name,"z_%d.%d_%d",i,j,t);
				z[i][j][t].setName(name);
			}
		}
	}
	for(t = 1; t <= T; t++) {
		for(n = 1; n <= N; n++) {
			masterModel.add(y[n][t] <= y[0][t]);        //���0�ڵ�û�У���ô������Ҳ������
		}
	}
	for(t = 1; t <= T; t++) 
	{
		IloExpr temp(masterEnv);
		for(i = 0; i <= N; i++)
			for(j = i + 1; j <= N; j++) 
				temp += (cost[i][j] * z[i][j][t]);
		masterModel.add(temp <= B[t]);
		temp.end();
		for(n = 0; n <= N; n++) {
			masterModel.add(z[n][n][t] == 0);
			for(i = n + 1; i <= N; i++) {
				masterModel.add(z[n][i][t] == z[i][n][t]);
			}
			IloExpr temp1(masterEnv);
			for(i = 0; i <= N; i++) {
				temp1 += z[n][i][t];
			}
			masterModel.add(temp1 == 2 * y[n][t]);
			temp1.end();
		}
    }
	masterModel.add(eta>=1e-6);
	masterModel.add(IloMinimize(masterEnv,eta));
	masterCplex.extract(masterModel);
}

//���� ������ģ�ԣ�ȱ��13��14ʽ
void Model::createWorkerLP()
{
	int i,j,k,t,n,m,tt,l;
	workerModel=IloModel(workerEnv);
	workerCplex=IloCplex(workerEnv);
	dualObj=IloObjective(workerEnv);
	dualObj.setSense(IloObjective::Maximize);
	workerModel.add(dualObj);
	initWorkerVariable();
	printf("finished init worker variables\n");
	IloNum num=0;
	try
	{
		for(n=1;n<=N;n++)
		{
			for(t=2;t<=T;t++)
			{
				IloExpr temp29(workerEnv),temp30(workerEnv);
				temp29+=theta_up_n1t[n][t];
				temp30+=theta_low_n1t[n][t];
				for(k=1;k<=K;k++)
				{
					temp29-=theta_up_nk2t[k][n][t];
					temp30-=theta_low_nk2t[k][n][t];
				}
				workerModel.add(temp29==0);  //  29
				workerModel.add(temp30==0);  //  30
				temp29.end();
				temp30.end();
			}
		}

		for(n=1;n<=N;n++)
		{
			for(t=2;t<=T;t++)   //��Ÿо���2��ʼ��ԭ����1
			{
				IloExpr temp42(workerEnv),temp44(workerEnv);
				temp42+=(phi_low_n2t[n][t]-phi_up_n2t[n][t]);
				temp44+=delta[n][t];
				for(k=1;k<=K;k++)
				{
					temp44+=b[k]*(theta_up_nk2t[k][n][t]+theta_low_nk2t[k][n][t]);
					for(m=t+1;m<=T;m++)  temp42+=a[k]*(theta_low_nk2t[k][n][m]-theta_up_nk2t[k][n][m]);
				}
				IloRange rng;
				//workerModel.add(temp42==0);  
				rng=IloRange(workerEnv,0,temp42,0);
				q_nt_Rngs[n][t]=rng;
				workerModel.add(rng);  //42

				rng=IloRange(workerEnv,1.0,temp44,1.0);
				alphaRngs[n][t]=rng;
				workerModel.add(rng);  //  44
				temp42.end();
				temp44.end();
			}
		}
		for(n=1;n<=N;n++)
		{
			IloExpr temp41(workerEnv);
			for(t=2;t<=T;t++){
				for(k=1;k<=K;k++) temp41+=a[k]*(theta_low_nk2t[k][n][t]-theta_up_nk2t[k][n][t]);
			}
			temp41-=phi_n1[n];
			IloRange rng=IloRange(workerEnv,temp41,0);
			q_nt_Rngs[n][1]=rng;
			workerModel.add(rng);  //41

			temp41.end();
		}
		for(n=1;n<=N;n++)
		{
			for(i=1;i<=N;i++)
			{
				for(t=2;t<=T;t++)
				{
					for(tt=1;tt<=T;tt++)
					{
						IloExpr temp31(workerEnv),temp32(workerEnv);
						workerModel.add(phi_up_n2t[n][t]*d_low[i][tt]-phi_up_ni3t[n][i][t][tt]<=0);  //37
						workerModel.add(phi_low_n2t[n][t]*d_low[i][tt]-phi_low_ni3t[n][i][t][tt]<=0);  //38
						workerModel.add(-phi_up_n2t[n][t]*d_up[i][tt]+phi_up_ni3t[n][i][t][tt]<=0);  //39
						workerModel.add(-phi_low_n2t[n][t]*d_up[i][tt]+phi_low_ni3t[n][i][t][tt]<=0);  //40

						temp31+=theta_up_n1t[n][t]*mu[i][tt];
						temp32+=theta_low_n1t[n][t]*mu[i][tt];
						for(k=1;k<=K;k++)
						{
							temp31-=theta_up_nik3t[k][n][i][t][tt];
							temp32-=theta_low_nik3t[k][n][i][t][tt];
						}
						workerModel.add(temp31==0);   //31
						workerModel.add(temp32==0);   //32
					}
				}
			}
		}
		for(n=1;n<=N;n++)
		{
			for(i=1;i<=N;i++)
			{
				for(t=2;t<=T;t++)  //ԭ�ı�ע��1��ʼ���о��ô�2��ʼ
				{
					for(tt=1;tt<=T;tt++)
					{
						IloExpr temp43(workerEnv);
						temp43+=(phi_low_ni3t[n][i][t][tt]-phi_up_ni3t[n][i][t][tt]);
						for(k=1;k<=K;k++){
							for(m=t+1;m<=T;m++) temp43+=a[k]*(theta_low_nik3t[k][n][i][m][tt]-theta_up_nik3t[k][n][i][m][tt]);
						}
						for(l=t;l<=T;l++) temp43+=lambda[n][i][t][l]*e[l][tt];
						//�����޸�
						//if(tt>=t) temp43+=lambda[n][i][t][tt];
						IloRange rng=IloRange(workerEnv,0,temp43,0);
						q_nit_tt[n][i][t][tt]=rng;
						workerModel.add(rng);   //43
						temp43.end();
					}
				}
			}
		}
		for(k=1;k<=K;k++)
		{
			for(n=1;n<=N;n++)
			{
				for(i=1;i<=N;i++)
				{
					for(t=2;t<=T;t++)
					{
						for(tt=1;tt<=T;tt++)
						{
							workerModel.add(theta_up_nk2t[k][n][t]*d_low[i][tt]-theta_up_nik3t[k][n][i][t][tt]<=0); //33
							workerModel.add(theta_low_nk2t[k][n][t]*d_low[i][tt]-theta_low_nik3t[k][n][i][t][tt]<=0); //34
							workerModel.add(-theta_up_nk2t[k][n][t]*d_up[i][tt]+theta_up_nik3t[k][n][i][t][tt]<=0);   //35
							workerModel.add(-theta_low_nk2t[k][n][t]*d_up[i][tt]+theta_low_nik3t[k][n][i][t][tt]<=0);  //36
						}
					}
				}
			}
		}
		workerCplex.extract(workerModel);
	}
	catch ( IloException& e )
	{
		std::cout << e << std::endl;
		e.end();
	}
	catch ( ... )
	{
		std::cout << "Unknown exception\n";
	}
}

//������y��ֵ��֪�����¹��� ������
void Model::rebuildWorkerLP(const IloNumMatrix &yy)
{
	int i,j,t,k,tt,n,m;
	workerModel.remove(dualObj);
	IloExpr objExpr=dualObj.getExpr();
	objExpr.clear();
	IloNum num=0;
	for(k=1;k<=K;k++) //����Ŀ�꺯��
	{
		for(t=2;t<=T;t++)
		{
			for(n=1;n<=N;n++)
			{
				objExpr-=a[k]*tau_up[n][t]*theta_up_nk2t[k][n][t];//1
				objExpr+=a[k]*tau_low[n][t]*theta_low_nk2t[k][n][t];//3
			}
		}
	}
	for(k=1;k<=K;k++)
	{
		for(t=2;t<=T;t++)
		{
			for(tt=1;tt<=T;tt++)
			{
				num=0;
				for(m=1;m<=t-1;m++) num+=e[m][tt];
				num*=a[k];
				for(n=1;n<=N;n++)
				{
					objExpr-=num*theta_up_nik3t[k][n][n][t][tt];  //2
					objExpr+=num*theta_low_nik3t[k][n][n][t][tt]; //4
				}
			}
		}
	}
	for(n=1;n<=N;n++)
	{
		objExpr-=M*yy[n][1]*phi_n1[n];    //5
		for(t=2;t<=T;t++)
		{
			objExpr-=M*yy[n][t]*phi_up_n2t[n][t];  //6
			objExpr+=epsilon*delta[n][t];      //7
		}
	}
	dualObj.setExpr(objExpr);
	workerModel.add(dualObj);
	objExpr.end();
}

IloExpr Model::buildOptimalBendersExpr()
{
	int n,m,t,k,tt;
	IloNum val=-1,num;
	IloExpr temp(masterEnv);
	IloNum b=0;
	for(k=1;k<=K;k++) //����benders cut
	{
		for(t=2;t<=T;t++)
		{
			for(n=1;n<=N;n++)
			{
				val=workerCplex.getValue(theta_up_nk2t[k][n][t]);
				val=val*a[k]*tau_up[n][t]; 
				b-=val;  //1      ���cut ��Ӧ������ 1-7��������д˳��

				val=workerCplex.getValue(theta_low_nk2t[k][n][t]);
				val=val*a[k]*tau_low[n][t];
				b+=val;  //3
			}
		}
	}
	for(k=1;k<=K;k++)
	{
		for(t=2;t<=T;t++)
		{
			for(tt=1;tt<=T;tt++)
			{
				num=0;
				for(m=1;m<=t-1;m++) num+=e[m][tt];
				num=num*a[k];

				val=0;
				for(n=1;n<=N;n++)  val+=workerCplex.getValue(theta_up_nik3t[k][n][n][t][tt]);
				b-=num*val; //2

				val=0;
				for(n=1;n<=N;n++)  val+=workerCplex.getValue(theta_low_nik3t[k][n][n][t][tt]);
				b+=num*val; //4
			}
		}
	}
	for(n=1;n<=N;n++)
	{
		temp-=M*y[n][1]*workerCplex.getValue(phi_n1[n]); //5
		for(t=2;t<=T;t++)
		{
			temp-=M*y[n][t]*workerCplex.getValue(phi_up_n2t[n][t]);    //6
			b+=epsilon*workerCplex.getValue(delta[n][t]);          //7
		}
	}
	//cout<<"add benders cut:";
	//printExpr(temp);
	//cout<<"+"<<b<<"<=eta"<<endl;
	temp+=b;
	return temp;
}

IloExpr Model::buildInfeasibleBendersExpr()
{
	IloNumVarArray vars(workerEnv);
	IloNumArray vals(workerEnv);
	workerCplex.getRay(vals,vars);  //��ȡ��ż����ļ���
	IloExpr temp(masterEnv);
	Info *info;
	void* obj;
	int i,j,m,k,n,t,tt;
	IloNum num,val,b=0;
	for(i=0;i<vars.getSize();i++)
	{
		//cout<<vars[i].getName()<<"="<<vals[i]<<endl;
		obj=(void*)vars[i].getObject();
		if(obj!=NULL) //��������
		{
			info=(Info *)obj;
			k=info->k;
			n=info->n;
			t=info->t;
			tt=info->tt;
			//cout<<"type="<<info->type<<",k="<<k<<",n="<<n<<",t="<<t<<",tt="<<tt<<",val="<<vals[i]<<endl;
			switch(info->type)
			{
			case Info::varType::theta_up_nk2t:
				{
					b-=vals[i]*a[k]*tau_up[n][t];     //1
					break;
				}
			case Info::varType::theta_low_nk2t:
				{
					b+=vals[i]*a[k]*tau_low[n][t];   //3
					break;
				}
			case Info::varType::theta_up_nik3t:
				{
					num=0;
					for(m=1;m<=t-1;m++) num+=e[m][tt];
					num*=a[k];
					b-=num*vals[i];                         // 2
					break;
				}
			case Info::varType::theta_low_nik3t:
				{
					num=0;
					for(m=1;m<=t-1;m++) num+=e[m][tt];
					num*=a[k];
					b+=num*vals[i];                             // 4
					break;
				}
			case Info::varType::phi_n1:
				{
					temp-=M*y[n][1]*vals[i];      //5
					break;
				}
			case Info::varType::phi_up_n2t:
				{
					temp-=M*y[n][t]*vals[i];       //6
					break;
				}
			case Info::varType::delta_nt:
				{
					b+=epsilon*vals[i];  //7
					break;
				}
			}
		}
	}
	vars.end();
	vals.end();
	//cout<<"add benders cut:";
	//printExpr(temp);
	//cout<<"+"<<b<<"<=0"<<endl;
	temp+=b;
	return temp;
}

void Model::initWorkerVariable()
{
	int i,j,n,k,t,tt;
	Info *info;
	char name[40];
	e=IloNumMatrix(workerEnv, T + 1); //��λ����
	for(t = 0; t <= T; t++) {
		e[t] = IloNumArray(workerEnv, T + 1);
		for(tt = 0; tt <= T; tt++) {
			if(tt == t) {
				e[t][tt] = 1.0;
			} else {
				e[t][tt] = 0;
			}
		}
	}
	phi_n1=IloNumVarArray(workerEnv,N+1,0,IloInfinity); //5
	for(n=1;n<=N;n++){
		info=new Info(Info::phi_n1,n);
		Infos.push_back(info);
		phi_n1[n].setObject(info);
		sprintf(name,"phi_n1..%d",n);
		phi_n1[n].setName(name);
	}
	
	theta_up_n1t=IloNumVarMatrix(workerEnv,N+1);
	theta_low_n1t=IloNumVarMatrix(workerEnv,N+1);
	phi_up_n2t=IloNumVarMatrix(workerEnv,N+1);  //6
	phi_low_n2t=IloNumVarMatrix(workerEnv,N+1);
	delta=IloNumVarMatrix(workerEnv,N+1); //7

	alphaRngs=RangeMatrix(workerEnv,N+1);
	q_nt_Rngs=RangeMatrix(workerEnv,N+1);
	for(n=1;n<=N;n++)
	{
		alphaRngs[n]=IloRangeArray(workerEnv,T+1);
		q_nt_Rngs[n]=IloRangeArray(workerEnv,T+1);

		theta_up_n1t[n]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);
		theta_low_n1t[n]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);
		phi_up_n2t[n]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);  //6
		phi_low_n2t[n]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);
		delta[n]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);  //7
		for(t=1;t<=T;t++)
		{
			info=new Info(Info::phi_up_n2t,n,t);
			Infos.push_back(info);
			phi_up_n2t[n][t].setObject(info);

			info=new Info(Info::delta_nt,n,t);
			Infos.push_back(info);
			delta[n][t].setObject(info);

			if(t>=2)
			{
				workerModel.add(theta_up_n1t[n][t]>=0);
				workerModel.add(theta_low_n1t[n][t]>=0);
				workerModel.add(phi_up_n2t[n][t]>=0);
				workerModel.add(phi_low_n2t[n][t]>=0);
				workerModel.add(delta[n][t]>=0);
			}
			sprintf(name,"theta_up_n1t..%d_%d",n,t);
			theta_up_n1t[n][t].setName(name);

			sprintf(name,"theta_low_n1t..%d_%d",n,t);
			theta_low_n1t[n][t].setName(name);

			sprintf(name,"phi_up_n2t..%d_%d",n,t);
			phi_up_n2t[n][t].setName(name);

			sprintf(name,"phi_low_n2t..%d_%d",n,t);
			phi_low_n2t[n][t].setName(name);

			sprintf(name,"delta_nt..%d_%d",n,t);
			delta[n][t].setName(name);
		}
	}

	theta_up_nk2t=IloNumVarMatrix3(workerEnv,K+1);  //1
	theta_low_nk2t=IloNumVarMatrix3(workerEnv,K+1); //3
	for(k=1;k<=K;k++)
	{
		theta_up_nk2t[k]=IloNumVarMatrix(workerEnv,N+1);
		theta_low_nk2t[k]=IloNumVarMatrix(workerEnv,N+1);
		for(n=1;n<=N;n++)
		{
			theta_up_nk2t[k][n]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);
			theta_low_nk2t[k][n]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);
			for(t=1;t<=T;t++)
			{
				info=new Info(Info::theta_up_nk2t,k,n,t);
				Infos.push_back(info);
				theta_up_nk2t[k][n][t].setObject(info);

				info=new Info(Info::theta_low_nk2t,k,n,t);
				Infos.push_back(info);
				theta_low_nk2t[k][n][t].setObject(info);
				if(t>=2)
				{
					workerModel.add(theta_up_nk2t[k][n][t]>=0);
					workerModel.add(theta_low_nk2t[k][n][t]>=0);
				}

				sprintf(name,"theta_up_nk2t..%d_%d_%d",k,n,t);
				theta_up_nk2t[k][n][t].setName(name);

				sprintf(name,"theta_low_nk2t..%d_%d_%d",k,n,t);
				theta_low_nk2t[k][n][t].setName(name);
			}
		}
	}

	phi_up_ni3t=IloNumVarMatrix4(workerEnv,N+1);
	phi_low_ni3t=IloNumVarMatrix4(workerEnv,N+1);
	lambda=IloNumVarMatrix4(workerEnv,N+1);
	q_nit_tt=RangeMatrix4(workerEnv,N+1);
	for(n=1;n<=N;n++)
	{
		phi_up_ni3t[n]=IloNumVarMatrix3(workerEnv,N+1);
		phi_low_ni3t[n]=IloNumVarMatrix3(workerEnv,N+1);
		lambda[n]=IloNumVarMatrix3(workerEnv,N+1);
		q_nit_tt[n]=RangeMatrix3(workerEnv,N+1);
		for(i=1;i<=N;i++)
		{
			phi_up_ni3t[n][i]=IloNumVarMatrix(workerEnv,T+1);
			phi_low_ni3t[n][i]=IloNumVarMatrix(workerEnv,T+1);
			lambda[n][i]=IloNumVarMatrix(workerEnv,T+1);
			q_nit_tt[n][i]=RangeMatrix(workerEnv,N+1);
			for(t=1;t<=T;t++)
			{
				phi_up_ni3t[n][i][t]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity); 
				phi_low_ni3t[n][i][t]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity); 
				lambda[n][i][t]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);
				q_nit_tt[n][i][t]=IloRangeArray(workerEnv,T+1);
				for(tt=1;tt<=T;tt++)
				{
					sprintf(name,"phi_up_ni3t..%d_%d_%d_%d",n,i,t,tt);
					phi_up_ni3t[n][i][t][tt].setName(name);

					sprintf(name,"phi_low_ni3t..%d_%d_%d_%d",n,i,t,tt);
					phi_up_ni3t[n][i][t][tt].setName(name);

					sprintf(name,"lambda..%d_%d_%d_%d",n,i,t,tt);
					phi_up_ni3t[n][i][t][tt].setName(name);
				}
			}
		}
	}
	theta_up_nik3t=IloNumVarMatrix5(workerEnv,K+1);  //2
	theta_low_nik3t=IloNumVarMatrix5(workerEnv,K+1); //4
	for(k=1;k<=K;k++)
	{
		theta_up_nik3t[k]=IloNumVarMatrix4(workerEnv,N+1);
		theta_low_nik3t[k]=IloNumVarMatrix4(workerEnv,N+1);
		for(n=1;n<=N;n++)
		{
			theta_up_nik3t[k][n]=IloNumVarMatrix3(workerEnv,N+1);
			theta_low_nik3t[k][n]=IloNumVarMatrix3(workerEnv,N+1);
			for(i=1;i<=N;i++)
			{
				theta_up_nik3t[k][n][i]=IloNumVarMatrix(workerEnv,T+1);
				theta_low_nik3t[k][n][i]=IloNumVarMatrix(workerEnv,T+1);
				for(t=1;t<=T;t++)
				{
					theta_up_nik3t[k][n][i][t]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);
					theta_low_nik3t[k][n][i][t]=IloNumVarArray(workerEnv,T+1,-IloInfinity,IloInfinity);

					for(tt=1;tt<=T;tt++)
					{
						if(n==i)
						{
							info= new Info(Info::theta_up_nik3t,k,n,i,t,tt);
							Infos.push_back(info);
							theta_up_nik3t[k][n][i][t][tt].setObject(info);

							info= new Info(Info::theta_low_nik3t,k,n,i,t,tt);
							Infos.push_back(info);
							theta_low_nik3t[k][n][i][t][tt].setObject(info);
						}

						sprintf(name,"theta_up_nik3t..%d_%d_%d_%d_%d",k,n,i,t,tt);
						theta_up_nik3t[k][n][i][t][tt].setName(name);

						sprintf(name,"theta_low_nik3t..%d_%d_%d_%d_%d",k,n,i,t,tt);
						theta_low_nik3t[k][n][i][t][tt].setName(name);
					}
				}
			}
		}
	}
}

void Model::readLine(char* chs,double *data,int n)
{
	int i=0,j=0,k=0;
	double x,y,tmp;
	char c;
	while(true){
		c=chs[i];
		if(k==n||c=='\r'||c=='\n')
			break;
		if(c>='0'&&c<='9')  //��������
		{  
			x=c-'0';
			for(j=i+1;chs[j]>='0'&&chs[j]<='9';j++)
				x=x*10+chs[j]-'0';
			i=j;
			y=0;
			if(chs[i]=='.'){  //С������
				tmp=1;
				for(j=i+1;chs[j]>='0'&&chs[j]<='9';j++){
					tmp*=10;
					y+=(chs[j]-'0')/tmp;
				}
				i=j;
			}
			data[k]=x+y;
			k+=1;
			continue;
		}
		i+=1;
	}
}
//��ȡ���ݣ����ݸ�ʽΪ
// ��һ�� ������ K N T ��ֵ
// �ڶ��� a ��ֵ������ K ����
// ������ b ��ֵ������ K ����
// tau_low��ֵ��N�У�ÿ�� T����
// tau_up ��ֵ��N�У�ÿ�� T����
// d_low ��ֵ��N�У�ÿ�� T����
// d_up��ֵ��N�У�ÿ�� T����
// mu ��ֵ��N�У�ÿ��T����
// cost��ֵ��N�У�ÿ��N����
// ÿ��Ԥ��ֵ һ�� T����
//Ϊ�˽�ģ�ͷ��㣬�͹�ʽ��Ӧ���������������0λ�ö���ʹ�ã��±������1��ʼ,����cost������y������z������0��ʼ
void Model::readData(char *filename)
{
	int i,n;
	FILE *fp = fopen(filename, "r");
	if(fp==NULL){
		printf("No such a file or have no access to read the file");
		return;
	}else{
		printf("file %s is opened \n",filename);
	}
	double data[100];
	char line[2000];   //Ĭ��ÿ�����2000���ַ�
	fgets(line, 2000, fp);  //��һ�� K N T
	n=3;
	readLine(line,data,3);
	K=data[0];
	N=data[1];
	T=data[2];
	a.resize(K+1);
	b.resize(K+1);
	B.resize(T+1);
	tau_low.resize(N+1);
	tau_up.resize(N+1);
	d_low.resize(N+1);
	d_up.resize(N+1);
	mu.resize(N+1);
	cost.resize(N+1);
	for(i=1;i<N+1;i++){
		tau_low[i].resize(T+1);
		tau_up[i].resize(T+1);
		d_low[i].resize(T+1);
		d_up[i].resize(T+1);
		mu[i].resize(T+1);
	}
	for(i=0;i<N+1;i++)
		cost[i].resize(N+1);
	fgets(line, 2000, fp);  //��ȡ a
	readLine(line,data,K);
	copyData(a,data,1,K);

	fgets(line, 2000, fp);  //��ȡ b
	readLine(line,data,K);
	copyData(b,data,1,K);

	for(i=1;i<N+1;i++)  //��ȡtau_low
	{
		fgets(line, 2000, fp);
		readLine(line,data,T);
		copyData(tau_low[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡtau_up
	{
		fgets(line, 2000, fp);
		readLine(line,data,T);
		copyData(tau_up[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡd_low
	{
		fgets(line, 2000, fp);
		readLine(line,data,T);
		copyData(d_low[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡd_up
	{
		fgets(line, 2000, fp);
		readLine(line,data,T);
		copyData(d_up[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡmu
	{
		fgets(line, 2000, fp);
		readLine(line,data,T);
		copyData(mu[i],data,1,T);
	}
	for(i=0;i<N+1;i++)  //��ȡcost
	{
		fgets(line, 2000, fp);
		readLine(line,data,N+1);
		copyData(cost[i],data,0,N+1);
	}

	fgets(line, 2000, fp);  //��ȡ B
	readLine(line,data,T);
	copyData(B,data,1,T);
	fclose(fp);
}

bool Model::isSkipLine(char *str)
{
	if(str==NULL)
		return true;
	int i=0;
	bool skip=true;
	while(str[i]==' '||str[i]=='\r') i+=1;
	if(str[i]=='\n'|| str[i]=='#'){
		return true;
	}else{
		return false;
	}
}

void Model::getOneLineData(FILE *fp,char *line,double *data,int n)
{
	while(true)
	{
		fgets(line, 2000, fp);  //��ȡһ���ַ������2000��
		if(isSkipLine(line)) continue;
		readLine(line,data,n);
		break;
	}
}

void Model::readData2(char *filename)  //���������ļ�ʹ�� # ����ע��
{
	int i,n;
	FILE *fp = fopen(filename, "r");
	if(fp==NULL){
		printf("No such a file or have no access to read the file");
		return;
	}else{
		printf("file %s is opened \n",filename);
	}
	double data[100];
	char line[2000];   //Ĭ��ÿ�����2000���ַ�
	n=3;
	getOneLineData(fp,line,data,n);
	K=data[0];
	N=data[1];
	T=data[2];
	a.resize(K+1);
	b.resize(K+1);
	B.resize(T+1);
	tau_low.resize(N+1);
	tau_up.resize(N+1);
	d_low.resize(N+1);
	d_up.resize(N+1);
	mu.resize(N+1);
	cost.resize(N+1);
	for(i=1;i<N+1;i++){
		tau_low[i].resize(T+1);
		tau_up[i].resize(T+1);
		d_low[i].resize(T+1);
		d_up[i].resize(T+1);
		mu[i].resize(T+1);
	}
	for(i=0;i<N+1;i++)
		cost[i].resize(N+1);
	getOneLineData(fp,line,data,K);//��ȡ a
	copyData(a,data,1,K);

	getOneLineData(fp,line,data,K);  //��ȡ b
	copyData(b,data,1,K);

	for(i=1;i<N+1;i++)  //��ȡtau_low
	{
		getOneLineData(fp,line,data,T);
		copyData(tau_low[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡtau_up
	{
		getOneLineData(fp,line,data,T);
		copyData(tau_up[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡd_low
	{
		getOneLineData(fp,line,data,T);
		copyData(d_low[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡd_up
	{
		getOneLineData(fp,line,data,T);
		copyData(d_up[i],data,1,T);
	}

	for(i=1;i<N+1;i++)  //��ȡmu
	{
		getOneLineData(fp,line,data,T);
		copyData(mu[i],data,1,T);
	}
	for(i=0;i<N+1;i++)  //��ȡcost
	{
		getOneLineData(fp,line,data,N+1);
		copyData(cost[i],data,0,N+1);
	}

	getOneLineData(fp,line,data,T); //��ȡ B
	copyData(B,data,1,T);
	fclose(fp);
}

void Model::copyData(vector<double> &to,double * from,int beginIdx,int len)
{
	for(int i=0;i<len;i++)
		to[i+beginIdx]=from[i];
}

//used to debug
void Model::print()
{
	printf("M=%d,eps=%lf\n",M,epsilon);
	int i,j;
	ofstream f("tmp.txt");
	f<<K<<" "<<N<<" "<<T<<endl;
	for(i=1;i<a.size();i++)
		f<<a[i]<<" ";
	f<<endl;
	for(i=1;i<b.size();i++)
		f<<b[i]<<" ";
	f<<endl;
	for(i=1;i<tau_low.size();i++)
	{
		for(j=1;j<tau_low[i].size();j++)
			f<<tau_low[i][j]<<" ";
		f<<endl;
	}
	for(i=1;i<tau_up.size();i++)
	{
		for(j=1;j<tau_up[i].size();j++)
			f<<tau_up[i][j]<<" ";
		f<<endl;
	}
	
	for(i=1;i<d_low.size();i++)
	{
		for(j=1;j<d_low[i].size();j++)
			f<<d_low[i][j]<<" ";
		f<<endl;
	}
	for(i=1;i<d_up.size();i++)
	{
		for(j=1;j<d_up[i].size();j++)
			f<<d_up[i][j]<<" ";
		f<<endl;
	}

	for(i=1;i<mu.size();i++)
	{
		for(j=1;j<mu[i].size();j++)
			f<<mu[i][j]<<" ";
		f<<endl;
	}
	for(i=0;i<cost.size();i++)
	{
		for(j=0;j<cost[i].size();j++)
			f<<cost[i][j]<<" ";
		f<<endl;
	}
	for(i=1;i<B.size();i++)
		f<<B[i]<<" ";
	f<<endl;
}

void Model::showResult(char *filename)
{
	ofstream f(filename);
	int width=20;
	int precision=8;
	cout<<"Solution status:"<<masterCplex.getStatus()<<endl;
	cout<<"Run time:"<<runtime<<endl;
	cout<<"Optimal value:"<<masterCplex.getObjValue()<<endl;
	cout<<"MIPRelativeGap:"<<masterCplex.getMIPRelativeGap()<<endl;
	
	f<<setiosflags(ios::left)<<setw(width)<<"Solution status:"<<masterCplex.getStatus()<<endl;
	f<<setiosflags(ios::left)<<setw(width)<<"Run time:"<<runtime<<endl;
	f<<setiosflags(ios::left)<<setw(width)<<"Optimal value:"<<masterCplex.getObjValue()<<endl;
	f<<setiosflags(ios::left)<<setw(width)<<"MIPRelativeGap:"<<masterCplex.getMIPRelativeGap()<<endl;
	

	int i,j,t,tt,n;
	IloNumMatrix data(masterEnv, N + 1);
	for(i = 0; i <= N; i++)  data[i]=IloNumArray(masterEnv, N + 1);
	for(t = 1; t <= T; t++) {
		for(i = 0; i <= N; i++) {
			if(abs(masterCplex.getValue(y[i][t])) < 0.1) {
				yy[i][t] = 0;
			} else {
				yy[i][t] = 1;
			}
		}
	}
	for(i = 0; i <= N; i++) {
		for(j = 0; j <= N; j++) {
			for(t = 1; t <= T; t++) {
				if(abs(masterCplex.getValue(z[i][j][t])) < 0.1) {
					zz[i][j][t] = 0;
				}else{
					zz[i][j][t] = 1;
				}
			}
		}
	}
	double total_cost = 0;
	for(t=1;t<=T;t++)
	{
		for(i = 0; i <= N; i++) {             //��ȡÿһ�ڵ�·��
			for(j = 0; j <= N; j++) {
				total_cost+=cost[i][j]*zz[i][j][t];
			}
		}
	}
	cout<<"Total cost:"<<setprecision(precision)<<total_cost<<endl;
	f<<setiosflags(ios::left)<<setw(width)<<"Total cost:"<<setprecision(precision)<<total_cost<<endl;
	rebuildWorkerLP(yy);           //�������һ��
	workerCplex.solve();

	IloNumMatrix d_real(workerEnv, N + 1);
	IloNumMatrix q_real(workerEnv, N + 1);
	IloNumMatrix x_real(workerEnv, N + 1);
	IloNumMatrix q_val(workerEnv, N + 1);
	IloNumMatrix4 qq(workerEnv, N + 1);
	for(i = 1; i <= N; i++) {
		d_real[i] = IloNumArray(workerEnv, T + 1);
		q_real[i] = IloNumArray(workerEnv, T + 1);
		x_real[i] = IloNumArray(workerEnv, T + 1);
		q_val[i]=IloNumArray(workerEnv, T + 1);
		qq[i] = IloNumMatrix3(workerEnv, N + 1);
		for(j = 1; j <= N; j++) {
			qq[i][j] = IloNumMatrix(workerEnv, T + 1);
			for(t = 1; t <= T; t++) {
				qq[i][j][t] = IloNumArray(workerEnv, T + 1);
			}
		}
	}
	
	for(i = 1; i <= N; i++) {
		q_val[i][1] = workerCplex.getDual(q_nt_Rngs[i][1]);
		for(t = 2; t <= T; t++) {
			q_val[i][t] = workerCplex.getDual(q_nt_Rngs[i][t]);
			for(j = 1; j <= N; j++) {
				for(tt = 1; tt <= T; tt++) {
					qq[i][j][t][tt] = workerCplex.getDual(q_nit_tt[i][j][t][tt]);
				}
			}
		}
	}
	double prob=0,mag=0,inven=0;
	char *d_real_file_name="d_real.txt";
	ifstream d_real_infile(d_real_file_name);
	if(!d_real_infile) {
		cerr << "------------can't open file d_real.txt--------------" << endl;
	}else{
		for(n = 1; n <= M_test; n++) {
			for(t = 1; t <= T; t++) {
				for(i = 1; i <= N; i++) {
					d_real_infile >> d_real[i][t];
				}
			}
			for(i = 1; i <= N; i++) {
				x_real[i][1] = q_val[i][1] - d_real[i][1];
			}
			for(t = 2; t <= T; t++) {
				if(yy[0][t] >= 0.5) {
					for(i = 1; i <= N; i++) {
						IloNum q_d_sum = 0;
						for(j = 1; j <= N; j++) {
							for(tt = 1; tt <= T; tt++) {
								q_d_sum += (d_real[j][tt] * qq[i][j][t][tt]);
							}
						}
						q_real[i][t] = q_val[i][t] + q_d_sum;
						x_real[i][t] = x_real[i][t - 1] + q_real[i][t] - d_real[i][t];
					}
				} else {
					for(i = 1; i <= N; i++) {
						x_real[i][t] = x_real[i][t - 1] - d_real[i][t];
					}
				}
			}
			IloNum x_fre, x_mag, x_level;
			x_fre = x_mag = x_level = 0;
			for(t = 1; t < T; t++) {
				for(i = 1; i <= N; i++) {
					if(x_real[i][t] > tau_up[i][t]) {
						x_fre += 1;
						x_mag += ((x_real[i][t] - tau_up[i][t]) / (tau_up[i][t] - tau_low[i][t]));
					} else if(x_real[i][t] < tau_low[i][t]) {
						x_fre += 1;
						x_mag += ((tau_low[i][t] - x_real[i][t]) / (tau_up[i][t] - tau_low[i][t]));
					}
					x_level += (x_real[i][t] / tau_up[i][t]);
				}
			}

			prob += ((x_fre / N) / (T - 1));
			mag += ((x_mag / N) / (T - 1));
			inven += ((x_level / N) / (T - 1));
		}
		cout<<"prob:"<<prob/M_test<<endl;
		cout<<"mag:"<<mag/M_test<<endl;
		cout<<"inven:"<<inven/M_test<<endl;

		f<<setiosflags(ios::left)<<setw(width)<<"prob:"<<prob/M_test<<endl;
		f<<setiosflags(ios::left)<<setw(width)<<"mag:"<<mag/M_test<<endl;
		f<<setiosflags(ios::left)<<setw(width)<<"inven:"<<inven/M_test<<endl<<endl;
	}
	vector<vector<int> > connect;
	vector<int> tmp;
	
	vector<double> per_cost(T+1,0);
	width=15;
	for(t=1;t<=T;t++)
	{
		for(i = 0; i <= N; i++) {             //��ȡÿһ�ڵ�·��
			for(j = 0; j <= N; j++) {
				data[i][j] = zz[i][j][t];
				per_cost[t]+=cost[i][j]*zz[i][j][t];
			}
		}
		isConnect(data, connect, 1e-6);   //ͼ����ͨ
		f<<setiosflags(ios::left)<<"period "<<t<<":"<<endl;
		for(i = 0; i < connect.size(); i++) 
		{
			tmp=connect.at(i);
			for(j=0;j<tmp.size();j++){
				cout<<tmp[j]<<"-->";
				f<<tmp[j]<<"-->";
			}
			cout<<tmp[0]<<endl;
			f<<tmp[0]<<endl;
		}
		cout<<"Budget:"<<B[t]<<endl;
		cout<<"Cost:"<<per_cost[t]/2<<endl<<endl;
		
		f<<setiosflags(ios::left)<<setw(width)<<"Budget:"<<B[t]<<endl;
		f<<setiosflags(ios::left)<<setw(width)<<"Cost:"<<per_cost[t]/2<<endl<<endl;
	}
	//cout<<"the Total cost is "<<total_cost/2<<endl;
	total_cost=0;
	double val=0;
	cout<<"Optimal alpha:"<<endl;
	f<<"Optimal alpha:"<<endl;
	width=12;
	for(i=1;i<=N;i++)
	{
		for(t=2;t<=T;t++)
		{
			val=workerCplex.getDual(alphaRngs[i][t]);
			cout<<val<<" ";
			f<<setiosflags(ios::left)<<setw(width)<<val<<" ";
		}
		cout<<endl;
		f<<endl;
	}
	f.close();
	for(i=0;i<data.getSize();i++)
		data[i].end();
	data.end();
}

