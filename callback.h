#pragma once
#include <vector>
#include <ilcplex/ilocplex.h>
#include "type.h"
#include "Model.h"
#include <iostream>

typedef IloCplex::ControlCallbackI Control;
ILOSTLBEGIN
// usercutConstraints callback and lazyconstraintCallback master problem 

/*****************************************************************************
 �� �� ��  : isConnect
 ��������  : �����ߵľ����ж�����ͼ�Ƿ���ͨ����ͨ���� true,���򷵻� false, branch ����ÿ����ͨ��֧�Ľڵ���
 �������  : graph  ͼ�ı߾���    
             eps    С�����ֵ����Ϊ�ߵ�Ȩ��ֵ�� 0
 �������  : branch ÿ����ͨ��֧����֧��Ϊ branch.size(),ÿ��vector<int> �洢ÿ����ͨ��֧�Ľڵ�
 �� �� ֵ  : true:ͼ����ͨ��  false:ͼ����ͨ
*****************************************************************************/
bool isConnect(const IloNumMatrix graph, vector<vector<int>> &branch, double eps=1e-6)
{
	IloInt N = graph.getSize();
	IloInt i, j;
	bool *isVisited = new bool[N];
	for(i = 0; i < N; i++)
	{
		isVisited[i] = false;
	}
	branch.clear();
	bool flag = true;
	IloInt cur = -1;
	
	for(i = 0; i < N; i++)
	{
		if(!isVisited[i])  //��ǰ�ڵ�û�����ʹ�
		{
			cur = i;
			isVisited[cur] = true;
			vector<int> tmp;            // �洢��ǰ ��ͨ��֧ �ڵ�
			tmp.push_back(cur);
			j = 0;
			while(j < N)
			{
				if(!isVisited[j] && graph[cur][j] > eps)  // ���Դӽڵ� cur ���� j 
				{
					isVisited[j] = true;
					tmp.push_back(j);
					cur = j;
					j=0;
					continue;
				}
				j = j + 1;
			}
			if(tmp.size() >= 2)                  //�µ㲻����
				branch.push_back(tmp);
		}
	}
	delete [] isVisited;
	return branch.size() == 1;
}

/*****************************************************************************
 �� �� ��  : buildCircleExpr
 ��������  : ���� 23 ʽ��\sum_{ i,j \in A(S)} z_{ij}^t+2 \sum_{n \in S} y_n^t-2 y_k_t,��ȥʹ y_k^t ���� k
 �������  : Snode  ���㼯 S   
             z  ���� z
			 y  ���� y
			 yy ���� y ��ֵ
			 t  ʱ�䣬����
			 env ����

 �� �� ֵ  : ���ʽ 23ʽ
*****************************************************************************/
IloExpr buildCircleExpr(const vector<int>& Snode, const IloIntVarMatrix3& z, const IloIntVarMatrix& y, const IloNumMatrix& yy, IloInt t, IloEnv env)
{
	IloExpr temp(env);
	IloInt i, j;
	IloInt max_k = -1;        // ʹ y_k^t ����k
	IloNum val = -1e8;
	for(i = 0; i < Snode.size(); i++)
	{
		if(yy[Snode[i]][t] > val)
		{
			val = yy[Snode[i]][t];
			max_k = Snode[i];
		}
		for(j = 0; j < Snode.size(); j++)
		{
			if(i != j)
			{
				temp += z[Snode[i]][Snode[j]][t];   //\sum_{ i,j \in A(S)} z_{ij}^t
			}
		}
	}
	for(i = 0; i < Snode.size(); i++)
	{
		temp -= 2 * y[Snode[i]][t];     //  \sum_{n \in S} y_n^t
	}
	temp += 2 * y[max_k][t];
	return temp;
}

void printExpr(IloExpr expr){
	 typedef IloExpr::LinearIterator ExprItem;
	 ExprItem it=expr.getLinearIterator();
	 bool first=true;
	 while(it.ok()){
		 if(it.getCoef()>0){
			 if(!first){
				 cout<<"+";
				 first=false;
			 } 
			 cout<<it.getCoef()<<"*"<<it.getVar().getName();
		 }else{
			 cout<<it.getCoef()<<"*"<<it.getVar().getName();
		 }
		 ++it;
	 }
}
//������ﵽ���ţ���ȡ��Dual subproblem �ı���ֵ,������benders Optimal cut ��˱��ʽ

IloExpr buildMinCutExpr(const vector<vector<int> > &cons,const IloIntVarMatrix3& z,IloInt t,IloEnv env){
	IloExpr expr(env);
	int idx1,idx2;
	for(int i=0;i<cons[0].size();i++){
		for(int j=0;j<cons[1].size();j++){
			idx1=cons[0][i];
			idx2=cons[1][j];
			expr+=z[idx1][idx2][t];
		}
	}
	return expr;
}
//��ǰ �����������benders cut,��ʱ������ subtour cut
ILOUSERCUTCALLBACK1(myuserCut,Model &,model)
{
	if(!isAfterCutLoop())
		return;
	
	int i,j,t;
	int K=model.K,N=model.N,T=model.T;
	IloIntVarMatrix &y=model.y;
	IloNumMatrix &yy=model.yy;
	IloIntVarMatrix3 &z=model.z;  
	IloNumMatrix3 &zz=model.zz; 
	//printf("user cut calback,obj value=%lf\n",getObjValue());
	for(t=1;t<=T;t++)
	{
		//cout<<t<<":";
		for(i=0;i<=N;i++) 
		{
			yy[i][t]=getValue(y[i][t]);
			//if(yy[i][t]>=1e-10) cout<<"y("<<i<<")="<<yy[i][t]<<" ";
			for(j=0;j<=N;j++)
			{
				zz[i][j][t]=getValue(z[i][j][t]);
			}
		}
		//cout<<endl;
	}
	IloNum masterObjVal=getObjValue();
	model.rebuildWorkerLP(yy);
	//cout<<"try to solve the worker problem!!!"<<endl;
	model.workerCplex.solve();
	if(model.workerCplex.getStatus()==IloAlgorithm::Optimal)
	{
		IloNum workObjVal=model.workerCplex.getObjValue();
		//cout<<"masterObj="<<masterObjVal<<",workObj="<<workObjVal<<",diff="<<workObjVal-masterObjVal<<",";
		if(workObjVal-masterObjVal>=1e-1)  //���benders Cut,������з�֧
		{
			IloExpr expr=model.buildOptimalBendersExpr();
			add(expr<=model.eta);
			//cout<<"the value of the benders expression is:"<<getValue(expr)<<endl;
			expr.end();
			model.fraOptbendersCount+=1;
		}else{
			//cout<<"------------------begin branch and cut---------------"<<endl;
		}
	}else if(model.workerCplex.getStatus()==IloAlgorithm::Unbounded) //���bensrs cut
	{
		//cout<<"try to add Infeasible benders cut,user cut callback,masterObj="<<masterObjVal<<",";
		IloExpr expr=model.buildInfeasibleBendersExpr();
		//cout<<"the value of the benders expression is:"<<getValue(expr)<<endl;
		add(expr<=0);
		expr.end();
		model.fraInfbendersCount+=1;
	}else{
		cout<<"this problem is unbound!!!"<<endl;
	}
}

//����������,����� benders cut,����Ѿ����������⣬�������subtour cut
ILOLAZYCONSTRAINTCALLBACK1(mylazyCut,Model &,model)
{
	int i,j,t;
	int K=model.K,N=model.N,T=model.T;
	IloIntVarMatrix &y=model.y;
	IloNumMatrix &yy=model.yy;
	IloIntVarMatrix3 &z=model.z;  
	IloNumMatrix3 &zz=model.zz;
	for(t = 1; t <= T; t++) {
		//cout<<t<<":";
		for(i = 0; i <= N; i++) {
			if(abs(getValue(y[i][t])) < 0.1) {
				yy[i][t] = 0;
			} else {
				yy[i][t] = 1;
				//cout<<"y("<<i<<")="<<1<<" ";
			}
			for(j=0;j<=N;j++){
				if(abs(getValue(z[i][j][t])) < 0.1){
					zz[i][j][t]=0;
				}else{
					zz[i][j][t]=1;
				}
			}
		}
		//cout<<endl;
	}
	IloNum masterObjVal=getObjValue();
	model.rebuildWorkerLP(yy);

	model.workerCplex.solve();
	if(model.workerCplex.getStatus()==IloAlgorithm::Optimal)
	{
		IloNum workObjVal=model.workerCplex.getObjValue();
		
		if(workObjVal-masterObjVal>=1e-6)  //���benders Cut,�������Ƿ����subtour
		{
			//cout<<"worker problem status:"<<model.workerCplex.getStatus()<<",workerobj="<<workObjVal<<",master Obj="<<masterObjVal<<",diff="<<workObjVal-masterObjVal<<endl;
			//cout<<"lazy constraints callback,try to add benders Optimal cut,";
			IloExpr temp=model.buildOptimalBendersExpr();
			//cout<<"the value of the benders expression is:"<<getValue(temp)<<endl;
			add(temp<=model.eta); 
			temp.end();
			model.intOptbendersCount+=1;
		}else{  //�õ�һ�������⣬���ǿ��ܲ�����subtour constraints
			//cout<<"check for subtour cut"<<endl;
			IloNumMatrix data(getEnv(), N + 1);
			for(i = 0; i <= N; i++)  data[i]=IloNumArray(getEnv(), N + 1);
			for(t = 1; t <= T; t++)
			{
				for(i = 0; i <= N; i++) {             //��ȡÿһ�ڵ�·��
					for(j = 0; j <= N; j++) data[i][j] = zz[i][j][t];
				}
				vector<vector<int> > connect;
				if(!isConnect(data, connect, 1e-6))   //ͼ����ͨ
				{
					vector<int> tmp;
					for(i = 1; i < connect.size(); i++)   // �������� 0 �Ľڵ�
					{
						tmp = connect.at(i);
						IloExpr expr = buildCircleExpr(tmp, z, y, yy, t, getEnv());
						if(getValue(expr) > 1e-4) {
							add(expr <= 0);
							model.subtourCutCount+=1;
							//cout << "Integer node, user cut ,subtour cut is added, " ;
							//printExpr(expr);
							//cout<<"<=0"<<endl;
						}
						expr.end();
					}
				}
			}
			for(i=0;i<data.getSize();i++)
				data[i].end();
			data.end();
		}
	}else if(model.workerCplex.getStatus()==IloAlgorithm::Unbounded) //���bensrs cut
	{
		//cout<<"lazy constraints callback In Integer node,try to add benders Infeasible cut";
		IloExpr temp=model.buildInfeasibleBendersExpr();
		add(temp<=0);
		//cout<<"the value of the benders expression is:"<<getValue(temp)<<endl;
		temp.end();
		model.intInfbendersCount+=1;
	}else{
		cout<<"this problem is unbound!!!"<<endl;
	}
}

ILOUSERCUTCALLBACK1(myuserCut_0_node,Model &,model)
{
	if(!isAfterCutLoop())
		return;
	if(getNnodes()!=0) return;
	int i,j,t;
	int K=model.K,N=model.N,T=model.T;
	IloIntVarMatrix &y=model.y;
	IloNumMatrix &yy=model.yy;
	IloIntVarMatrix3 &z=model.z;  
	IloNumMatrix3 &zz=model.zz; 
	//printf("user cut calback,obj value=%lf\n",getObjValue());
	for(t=1;t<=T;t++)
	{
		//cout<<t<<":";
		for(i=0;i<=N;i++) 
		{
			yy[i][t]=getValue(y[i][t]);
			//if(yy[i][t]>=1e-10) cout<<"y("<<i<<")="<<yy[i][t]<<" ";
			for(j=0;j<=N;j++)
			{
				zz[i][j][t]=getValue(z[i][j][t]);
			}
		}
		//cout<<endl;
	}
	IloNum masterObjVal=getObjValue();
	model.rebuildWorkerLP(yy);
	//cout<<"try to solve the worker problem!!!"<<endl;
	model.workerCplex.solve();
	if(getNnodes()==0){
		if(model.workerCplex.getStatus()==IloAlgorithm::Optimal)
		{
			IloNum workObjVal=model.workerCplex.getObjValue();
			//cout<<"masterObj="<<masterObjVal<<",workObj="<<workObjVal<<",diff="<<workObjVal-masterObjVal<<",";
			if(workObjVal-masterObjVal>=1e-2)  //���benders Cut,������з�֧
			{
				IloExpr expr=model.buildOptimalBendersExpr();
				add(expr<=model.eta);
				//cout<<"the value of the benders expression is:"<<getValue(expr)<<endl;
				expr.end();
				model.fraOptbendersCount+=1;
			}else{
				//cout<<"------------------begin branch and cut---------------"<<endl;
			}
		}else if(model.workerCplex.getStatus()==IloAlgorithm::Unbounded) //���bensrs cut
		{
			//cout<<"try to add Infeasible benders cut,user cut callback,masterObj="<<masterObjVal<<",";
			IloExpr expr=model.buildInfeasibleBendersExpr();
			//cout<<"the value of the benders expression is:"<<getValue(expr)<<endl;
			add(expr<=0);
			expr.end();
			model.fraInfbendersCount+=1;
		}else{
			cout<<"this problem is unbound!!!"<<endl;
		}
	}
}


ILOLAZYCONSTRAINTCALLBACK1(mylazyCut_0_node,Model &,model)
{
	int i,j,t;
	int K=model.K,N=model.N,T=model.T;
	IloIntVarMatrix &y=model.y;
	IloNumMatrix &yy=model.yy;
	IloIntVarMatrix3 &z=model.z;  
	IloNumMatrix3 &zz=model.zz;
	for(t = 1; t <= T; t++) {
		//cout<<t<<":";
		for(i = 0; i <= N; i++) {
			if(abs(getValue(y[i][t])) < 0.1) {
				yy[i][t] = 0;
			} else {
				yy[i][t] = 1;
				//cout<<"y("<<i<<")="<<1<<" ";
			}
			for(j=0;j<=N;j++){
				if(abs(getValue(z[i][j][t])) < 0.1){
					zz[i][j][t]=0;
				}else{
					zz[i][j][t]=1;
				}
			}
		}
		//cout<<endl;
	}
	IloNum masterObjVal=getObjValue();
	//printf("lazy constriants calback,master obj value=%lf\n",masterObjVal);
	model.rebuildWorkerLP(yy);
	//cout<<"try to solve the worker problem!!!"<<endl;
	model.workerCplex.solve();

	if(model.workerCplex.getStatus()==IloAlgorithm::Optimal)
	{
		IloNum workObjVal=model.workerCplex.getObjValue();

		if(workObjVal-masterObjVal>=1e-6)  //���benders Cut,�����ڵ�һ����
		{
			//cout<<"worker problem status:"<<model.workerCplex.getStatus()<<",workerobj="<<workObjVal<<",master Obj="<<masterObjVal<<",diff="<<workObjVal-masterObjVal<<endl;
			//cout<<"lazy constraints callback,try to add benders Optimal cut,";
			IloExpr temp=model.buildOptimalBendersExpr();
			//cout<<"the value of the benders expression is:"<<getValue(temp)<<endl;
			add(temp<=model.eta); 
			temp.end();
			model.intOptbendersCount+=1;
		}else{  //�õ�һ�������⣬���ǿ��ܲ�����subtour constraints
			//cout<<"check for subtour cut"<<endl;
			IloNumMatrix data(getEnv(), N + 1);
			for(i = 0; i <= N; i++)  data[i]=IloNumArray(getEnv(), N + 1);
			for(t = 1; t <= T; t++)
			{
				for(i = 0; i <= N; i++) {             //��ȡÿһ�ڵ�·��
					for(j = 0; j <= N; j++) data[i][j] = zz[i][j][t];
				}
				vector<vector<int> > connect;
				if(!isConnect(data, connect, 1e-6))   //ͼ����ͨ
				{
					vector<int> tmp;
					for(i = 1; i < connect.size(); i++)   // �������� 0 �Ľڵ�
					{
						tmp = connect.at(i);
						IloExpr expr = buildCircleExpr(tmp, z, y, yy, t, getEnv());
						if(getValue(expr) > 1e-4) {
							add(expr <= 0);
							model.subtourCutCount+=1;
							//cout << "Integer node, user cut ,subtour cut is added, "<<endl ;
							//printExpr(expr);
							//cout<<"<=0"<<endl;
						}
						expr.end();
					}
				}
			}
			for(i=0;i<data.getSize();i++)
				data[i].end();
			data.end();
		}
	}else if(model.workerCplex.getStatus()==IloAlgorithm::Unbounded) //���bensrs cut
	{
		//cout<<"lazy constraints callback In Integer node,try to add benders Infeasible cut";
		IloExpr temp=model.buildInfeasibleBendersExpr();
		add(temp<=0);
		//cout<<"the value of the benders expression is:"<<getValue(temp)<<endl;
		temp.end();
		model.intInfbendersCount+=1;
	}else{
		cout<<"this problem is unbound!!!"<<endl;
	}
	
	
}

//type: 0--> benders cut is added in all node, 1-->add only in root  2-->not add
ILOUSERCUTCALLBACK2(myuserCut_tour,Model &,model,int,type)
{
	if(!isAfterCutLoop())
		return;
	int i,j,t;
	int K=model.K,N=model.N,T=model.T;
	IloIntVarMatrix &y=model.y;
	IloNumMatrix &yy=model.yy;
	IloIntVarMatrix3 &z=model.z;  
	IloNumMatrix3 &zz=model.zz; 
	for(t=1;t<=T;t++)
	{
		for(i=0;i<=N;i++) 
		{
			yy[i][t]=getValue(y[i][t]);
			for(j=0;j<=N;j++) zz[i][j][t]=getValue(z[i][j][t]);
		}
	}
	IloNum masterObjVal=getObjValue();
	model.rebuildWorkerLP(yy);
	
	model.workerCplex.solve();
	bool isadd=false;
	if(type==0){
		isadd=true;
	}else if(type==1){
		if(getNnodes()==0){
			isadd=true;
		}
	}
	if(isadd){
		if(model.workerCplex.getStatus()==IloAlgorithm::Optimal)
		{
			IloNum workObjVal=model.workerCplex.getObjValue();
			if(workObjVal-masterObjVal>=1e-2)
			{
				IloExpr expr=model.buildOptimalBendersExpr();
				add(expr<=model.eta);
				expr.end();
				model.fraOptbendersCount+=1;
			}
		}else if(model.workerCplex.getStatus()==IloAlgorithm::Unbounded)
		{
			IloExpr expr=model.buildInfeasibleBendersExpr();
			add(expr<=0);
			expr.end();
			model.fraInfbendersCount+=1;
		}else{
			cout<<"this problem is unbound!!!"<<endl;
		}
	}
	double eps=0.15;
	if(model.wait->isAdd()){
		vector<vector<double> > data(N + 1);
		vector<vector<int> > con;
		for(i = 0; i <= N; i++)  data[i].resize(N + 1);
		Maxflow *flow=model.f;
		double result=0;
		int idx;
		bool flag;
		for(t=1;t<=T;t++){
			for(i=0;i<=N;i++){
				for(j=0;j<=N;j++){
					data[i][j]=zz[i][j][t];
				}
			}
			flow->resetGraph(data);
			flow->getSubGraph(con);
			flag=false;
			for(i=1;i<con.size();i++){
				if(con[i].size()>=2){
					IloExpr expr = buildCircleExpr(con[i], z, y, yy, t, getEnv());
					if(getValue(expr) > 1e-2) {
						add(expr<=0).end();
						//addLocal(expr<=0).end();
						//cout<<"not connect cut"<<endl;
					}
					flag=true;
				}
			}
			if(flag) return;
			for(i=1;i<=N;i++)
			{
				flow->resetGraph(data);
				result=flow->flow(0,i);
				if(result<2-eps)
				{
					flow->getSep(con);
					vector<int> &sink=con[1];
					if(sink.size()>=2&&sink.size()<N)
					{
						for(j=0;j<sink.size();j++)
						{
							idx=sink[j];
							if(result+eps<2*yy[idx][t])
							{
								IloExpr expr=buildMinCutExpr(con,z,t,getEnv());
								//addLocal(expr >= 2 * y[idx][t]).end();
								add(expr >= 2 * y[idx][t]).end();
								//cout<<"s t cut is added"<<endl;
							}
						}
					}
				}
			}
		}
	}
}
