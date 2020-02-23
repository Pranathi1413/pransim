#include <iostream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <unordered_map>
#include <cstring>
#include <string>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <bits/stdc++.h>
using namespace std;
using namespace boost::algorithm;
int r[32]={0};
char d[4096]={0};
int point=0; int hi;
vector<vector<string>> t={{"add","sub","slt"},{"addi","sll"},{"lw","sw"},{"bne","beq"},{"j"},{"la","li"}};
vector<int> tn={4,4,3,4,2,3};
unordered_map <string,int> labelmap;
unordered_map <string,int> dataseg;
struct instr
{
	int n,type,reg[3],op;
	string label,jlabel;
	int imm;

	void disp()
	{
		std::cout<<n<<" "<<type<<" "<<op;
		std::cout<<"\nreg:"<<reg[0]<<" "<<reg[1]<<" "<<reg[2];
		std::cout<<"\nlabels:"<<label<<" "<<jlabel<<" "<<imm<<endl;
	}
}I[50];

int findinmap(unordered_map<string,int> m,string key)
{
	for(auto i:m)
		if(i.first==key)
			return 1;
	return 0;
}

/*int findinvector(vector<string> v,string key)
{
	for(int i=0;i<v.size();i++)
		if
}*/
int labelcheck(int n)
{
	for(int i=0;i<n;i++)
		if(I[i].jlabel!="\0")
			{
				auto pos=findinmap(labelmap,I[i].jlabel);
				if(pos)
					I[i].imm = labelmap[I[i].jlabel] - I[i].n;
				else return 0;
				
			}
	return 1;
}

int regcheck(string reg)
{	if(reg.length()!=2) return -1;
	if(reg[0]=='r' || reg[0]=='R')
		{
			int r=stoi(reg.substr(1,reg.length()-1));
			if(r>=0 && r<=31)
				return r;
			else 
				return -1;
		}
	return -1;
	
	
}

int fill(int n,int ptr)
{	
	if(ptr+4>4096)
		return 0;
	char *a=(char *)&n;
	for (int i=3; i>=0; --i)
	{
		d[ptr+i]=*a;
		a++;
	}
	return 1;
}

int get(int ptr)
{
	
	int n; char *a=(char *)&n;
	for(int i=3; i>=0; i--)
		{
			*a=d[ptr+i];
			a++;
		}
	return n;
}

int numcheck (string temp)
{
	for(int p=0; p<temp.length(); p++)
				if(!(temp[p]>='0' && temp[p]<='9' || p==0 && (temp[p]=='+' || temp[p]=='-')))
					return 0;
	return 1;
}

int numcheckhex (string temp)
{
	for(int p=0; p<temp.length(); p++)
				if(!(temp[p]>='0' && temp[p]<='9' || temp[p]>='a' && temp[p]<='f'))
					return 0;
	return 1;
}

int parse(string str,int c)
{
	int i=0,j,k;
	I[c].n=c;
	vector<string> result; 
    boost::algorithm::split(result, str, is_any_of(" ,\t")); 
	/*std::cout<<endl<<result.size();
	for(int l=0;l<result.size();l++)
		{std::cout<<result[l]<<" ";
		if(result[l]=="\0") std::cout<<"null ";
		if(result[l]=="\n") std::cout<<"newline ";
		}*/
	do
	{
		auto iter = find(result.begin(),result.end(),"\0");
		if(iter != result.end())
			result.erase(iter);
		else break;
	} while (1);
	
	char ch = result[0][result[0].length()-1];
	if(ch==':')
		{
			I[c].label=result[0].substr(0,result[0].length()-1);
			labelmap[I[c].label]=c;
			if(result.size() == 1)
			{
				I[c].type = -1;
				return 1;
			}
            i++;
		}
    if(result[0][0]=='#')
    {
        I[c].type = -1;
        return 1;
    }
    else if (ch==':' && result.size()>1 && result[1][0]=='#')
    {
        I[c].type = -1;
        return 1;
    }

	for( j=0; j<6; j++)
		{
			for( k=0; k < t[j].size(); k++)
				if(result[i]==t[j][k])
					{
						I[c].type = j;
						I[c].op = k;
						break;
					}
			if(k < t[j].size())
				break;
		}
	if(j==6 || result.size()<tn[j])
		return 0;
    
	i++;
	int p;
	vector<string> temp;
	switch(I[c].type)
	{
		case 0:
		case 1:
		case 3:
			for(p=i; p<i+2; p++)
				{
					int r=regcheck(result[p]);
					if(r==-1)
						return 0;
					I[c].reg[p-i]=r;
				}
			i+=2;
			break;
		case 2:
			I[c].reg[0]=regcheck(result[i]);
			if(I[c].reg[0]==-1) 
				return 0;
			boost::algorithm::split(temp,result[i+1],is_any_of(" ()"));
			if(numcheck(temp[0])==1)
				I[c].imm=stoi(temp[0]);  
			else return 0;
			I[c].reg[1]=regcheck(temp[1]);
			if(I[c].reg[1]==-1 || temp.size()!=3) return 0;
			i+=2;
			//if(i!=result.size()) return 0;
			break;
		case 4:
			I[c].jlabel = result[i];
			i++;
			//if(i!=result.size()) return 0;
			break;
		case 5:
			I[c].reg[0]=regcheck(result[i]);
			if(I[c].reg[0]==-1) 
				return 0;
			i++;
			if(I[c].op==0)
			{
				if(dataseg.find(result[i])!=dataseg.end())
					{
						I[c].imm=dataseg[result[i]];
						i++;
					}
				else return 0;
				//if(i!=result.size()) return 0;

			}
			break;
			

	}
	if(I[c].type==0)
		{
			I[c].reg[2]=regcheck(result[i]);
			if(I[c].reg[2]==-1) return 0;
			i++;
		}
	else if(I[c].type==1 || I[c].type==5 && I[c].op==1)
		{	
			if(numcheck(result[i])==1)
				I[c].imm=stoi(result[i]);
			else if(result[i].substr(0,2)=="0x" || result[i].substr(0,2)=="0X")
				{
					string fun=result[i].substr(2,result[i].length()-2);
					if(numcheckhex(fun))
						I[c].imm=strtoul(result[i].c_str(),0,16);
					else {return 0;}

				}
			else { return 0;}
			i++;
		}
	else if(I[c].type==3)
		{
			I[c].jlabel=result[i];
			i++;
		}
	if(ch==':' && i==tn[j]+1) return 1;
	else if(i==tn[j]) return 1;
    else if(result[i][0]=='#') return 1;
    else return 0;
}

int execute(int start, int n)
{
	int c=start,addr;
	do
	{
		switch(I[c].type)
		{
			case 0:
				switch(I[c].op)
				{
					case 0:
						r[I[c].reg[0]] = r[I[c].reg[1]] + r[I[c].reg[2]];
						break;
					case 1:
						r[I[c].reg[0]] = r[I[c].reg[1]] - r[I[c].reg[2]];
						break;
					case 2:
						if(r[I[c].reg[1]]<r[I[c].reg[2]])
							r[I[c].reg[0]] = 1;
						else
							r[I[c].reg[0]] = 0;
						break;
				}
				c++;
				break;
			case 1:
				switch(I[c].op)
				{
					case 0:
						r[I[c].reg[0]] = r[I[c].reg[1]] + I[c].imm;
						break;
					case 1:
						if(I[c].imm>=0)
							r[I[c].reg[0]] = r[I[c].reg[1]] << I[c].imm;
						else
							r[I[c].reg[0]] = r[I[c].reg[1]] >> (-I[c].imm);
						break;
				}
				c++;
				break;
			case 2:
				addr=r[I[c].reg[1]] + I[c].imm;
				switch(I[c].op)
				{
					case 0:
						if(addr+4<=4096)
						r[I[c].reg[0]] = get(addr);
						else return 0;
						break;
					case 1:
						if(fill(r[I[c].reg[0]],addr)==0)
							return 0;
						break;
				}
				c++;
				break;
			case 3:
				switch(I[c].op)
				{
					case 0:
						if(r[I[c].reg[0]]!=r[I[c].reg[0]])
							c+=I[c].imm;
						else c++;
						break;
					case 1:
						if(r[I[c].reg[0]]==r[I[c].reg[0]])
							c+=I[c].imm;
						else c++;
						break;					
				}
				break;
			case 4:
				switch(I[c].op)
				{
					case 0:
						c+=I[c].imm;
						break;
				}
				break;
			case 5:
				switch(I[c].op)
				{
					case 0: case 1:
						r[I[c].reg[0]] = I[c].imm;
						c++;
						break;					
				}
				break;
			case -1:
				c++;
				break;
		}
	if(c==n)break;
	} while (1);
	return 1;
}

int maincheck()
{
	auto itr = findinmap(labelmap,"main");
	if(itr)
		return labelmap["main"];
	else return -1;
}

int main()
{
	ifstream f("input.asm");
	string str;
	do
	{
		std::getline(f,str);
	}while(str==" " || str=="\t" || str=="\0" || str=="\n" || str[0]=='#');
	/*if(str!=".data")
		{
			std::cout<<"where s data segment";
			return 0;
		}*/
	vector<string> tmp;
	boost::algorithm::split(tmp,str,is_any_of(" \t"));

	if(tmp.size()==1 && tmp[0]!=".data" && tmp[0]!=".text")
		{
			std::cout<<"error data/text";
			return 0;
		}
	else if(tmp.size()>1 && tmp[1][0]!='#')
		return 0;
	
	do
	{
		std::getline(f,str);
		if(str=="\0" || str[0]=='#')
			continue;
		vector<string> temp;
		boost::algorithm::split(temp,str,is_any_of(" \t:"));
		do
		{
		auto iter = find(temp.begin(),temp.end(),"\0");
		if(iter == temp.end())
			break;
		temp.erase(iter);
		} while (1);
		if(temp.size()>1 && temp[1][0]!='#')
			{
				std::cout<<"error";
				return 0;
			}
		if(temp[0]==".text")
			break;
		do{
		std::getline(f,str); } while(str=="\0" || str=="\n" ||str[0]=='#');
		vector<string> temp1;
		boost::algorithm::split(temp1,str,is_any_of(" \t"));
		do
		{
		auto iter = find(temp1.begin(),temp1.end(),"\0");
		if(iter != temp1.end())
			temp1.erase(iter);
		else break;
		} while (1);
		int v=0;
		if(temp1[0]==".word")
		{
			for(int k=1;k<temp1.size();++k)
				{
					if(numcheck(temp1[k]))
					{
						std::cout<<temp1[k]<<" ";
						fill(stoi(temp1[k]),point+v);
						v+=4;
					}
					else if(temp1[k][0]!='#')
						return 0;
				}
		}
		else return 0;
		dataseg[temp[0]]=point;
		point+=v;
				
	} while (1);
	
	int c=0;
	while(std::getline(f,str))
	{	if(str=="\0" || str=="\n" ||str[0]=='#') continue;
		int p=parse(str,c);
		I[c].disp();
		if(p==0) 
			{
				std::cout<<"error on inst "<<c;
				return 0;
			}
		
		c++;
	}

	
	/*for(int i=0;i<5;i++)
		std::cout<<get(4*i)<<" ";*/
	int start = maincheck();
	if(start == -1)
		{std::cout<<"no main";
		return 0;
		}
	if(labelcheck(c)==0)
		{
			std::cout<<"labels not matched";
			return 0;
		}

	int check = execute(start,c);
	if(check==0)
		std::cout<<"Error";
	else
	{
		for(int i=0;i<32;i++)
			std::cout<<i<<" "<<r[i]<<endl;
		
	}

	

	
	return 0;
}
