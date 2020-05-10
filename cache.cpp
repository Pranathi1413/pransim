#include <iostream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <unordered_map>
#include <cstring>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cmath>
using namespace std;
using namespace boost::algorithm;
int total_mem_acc = 0, l1_miss = 0, l2_miss = 0;
int total_instr = 0;
int bl_size, bl_spill = 0;
int mem_stall;
int r[32] = {0};
char d[4096] = {0};
int point = 0;
int pc, flag = 0;
int stallcount = 0;
vector<vector<string>> t = {{"add", "sub", "slt"}, {"addi", "sll"}, {"lw", "sw"}, {"bne", "beq"}, {"j"}, {"la", "li"}};
vector<int> tn = {4, 4, 3, 4, 2, 3};
unordered_map<string, int> labelmap;
unordered_map<string, int> dataseg;
int power(int base, int exp)
{
    return (int)(pow(base, exp));
}
class Cache
{
public:
    int size, set, assoc;
    int block, stall;
    int nindex, noffset, ntag;
    int *tag;
    int *time;
    char *dirty;
    char *data;

    void disp()
    {
        int blk = block / set;
        for (int i = 0; i < set; i++)
        {
            cout << endl
                 << "  set " << i << ":";
            for (int j = 0; j < blk; j++)
            {
                cout << endl
                     << "\ttag " << tag[i * blk + j] << ":  ";
                if (tag[i * blk + j] == -1)
                    continue;
                for (int p = 0; p < bl_size; p += 4)
                {
                    int n;
                    char *a = (char *)&n;
                    for (int k = 0; k <= 3; k++)
                    {
                        *a = data[(i * blk + j) * bl_size + p + k];
                        a++;
                    }
                    cout << n << "  ";
                }
            }
        }
    }
    char *find_in_cache(int addr, int write_y)
    {
        int index_chk = addr % power(2, nindex + noffset) / power(2, noffset);
        int tag_chk = addr / power(2, nindex + noffset);
        char *ptr = NULL;
        int blk = block / set;
        for (int i = 0; i < blk; i++)
        {
            if (tag[index_chk * blk + i] == tag_chk)
            {
                ptr = data + (blk * index_chk + i) * bl_size;
                for (int j = 0; j < blk; j++)
                {
                    if (time[index_chk * blk + j] < time[index_chk * blk + i] && time[index_chk * blk + j] != 0)
                        time[index_chk * blk + j]++;
                }
                time[index_chk * blk + i] = 1;
                if (write_y == 1)
                    dirty[index_chk * blk + i] = '1';
                break;
            }
        }
        return ptr;
    }
} l1, l2;
int evict_l2(int tag, int index, int blk)
{
    int p, max = 0;
    for (int i = 0; i < blk; i++)
        if (l2.time[index * blk + i] > max)
        {
            max = l2.time[index * blk + i];
            p = i;
        }
    int addr_to_evict = l2.tag[index * blk + p] * power(2, l2.nindex + l2.noffset) + index * power(2, l2.noffset);
    if (l2.dirty[index * blk + p] == '1')
    {
        for (int i = 0; i < bl_size; i++)
            d[addr_to_evict + i] = *(l2.data + (index * blk + p) * bl_size + i);
    }
    return p;
}
int evict_l1(int addr, int tag, int index, int blk)
{
    int p, max = 0;
    for (int i = 0; i < blk; i++)
        if (l1.time[index * blk + i] > max)
        {
            max = l1.time[index * blk + i];
            p = i;
        }
    int addr_to_evict = l1.tag[index * blk + p] * power(2, l1.nindex + l1.noffset) + index * power(2, l1.noffset);
    char *ptr_l2 = l2.find_in_cache(addr_to_evict, 1);
    if (ptr_l2 != NULL)
    {
        if (l1.dirty[index * blk + p] == '1')
        {
            for (int i = 0; i < bl_size; i++)
                *(ptr_l2 + i) = *(l1.data + (index * blk + p) * bl_size + i);
        }
    }
    else
    {
        int index_l2 = addr_to_evict % power(2, l2.nindex + l2.noffset) / power(2, l2.noffset);
        int tag_l2 = addr_to_evict / power(2, l2.nindex + l2.noffset);
        int blk_l2 = l2.block / l2.set;
        int i;
        for (i = 0; i < blk_l2; i++)
        {
            if (l2.tag[index_l2 * blk_l2 + i] == -1)
            {
                l2.tag[index_l2 * blk_l2 + i] = tag_l2;
                for (int j = 0; j < bl_size; j++)
                {
                    *(l2.data + (blk_l2 * index_l2 + i) * bl_size + j) = *(l1.data + (index * blk + p) * bl_size + j);
                }
                for (int j = 0; j < blk_l2; j++)
                {
                    if (l2.time[index_l2 * blk_l2 + j] != 0)
                        l2.time[index_l2 * blk_l2 + j]++;
                }
                l2.time[index_l2 * blk_l2 + i] = 1;
                l2.dirty[index_l2 * blk_l2 + i] = l1.dirty[index * blk + p];
                break;
            }
        }
        if (i == blk_l2)
        {
            int ptr = evict_l2(tag_l2, index_l2, blk_l2);

            for (int j = 0; j < bl_size; j++)
            {
                *(l2.data + (blk_l2 * index_l2 + ptr) * bl_size + j) = *(l1.data + (index * blk + p) * bl_size + j);
            }
            for (int j = 0; j < blk_l2; j++)
            {
                if (l2.time[index_l2 * blk_l2 + j] < l2.time[index_l2 * blk_l2 + ptr] && l2.time[index_l2 * blk_l2 + j] != 0)
                    l2.time[index_l2 * blk_l2 + j]++;
            }
            l2.tag[index_l2 * blk_l2 + ptr] = tag_l2;
            l2.time[index_l2 * blk_l2 + ptr] = 1;
            l2.dirty[index_l2 * blk_l2 + ptr] = l1.dirty[index * blk + p];
        }
    }
    return p;
}

void insert(int addr, int only_l1 = 0, char *ptr_l2 = NULL)
{
    int index_l1 = addr % power(2, l1.nindex + l1.noffset) / power(2, l1.noffset);
    int tag_l1 = addr / power(2, l1.nindex + l1.noffset);
    int blk_l1 = l1.block / l1.set;
    int index_l2 = addr % power(2, l2.nindex + l2.noffset) / power(2, l2.noffset);
    int tag_l2 = addr / power(2, l2.nindex + l2.noffset);
    int blk_l2 = l2.block / l2.set;
    int i, ptr;
    for (i = 0; i < blk_l1; i++)
    {
        if (l1.tag[index_l1 * blk_l1 + i] == -1)
        {
            l1.tag[index_l1 * blk_l1 + i] = tag_l1;
            for (int j = 0; j < bl_size; j++)
            {
                if (only_l1 == 0)
                    *(l1.data + (blk_l1 * index_l1 + i) * bl_size + j) = d[addr + j];
                else
                    *(l1.data + (blk_l1 * index_l1 + i) * bl_size + j) = *(ptr_l2 + j);
            }
            for (int j = 0; j < blk_l1; j++)
            {
                if (l1.time[index_l1 * blk_l1 + j] != 0)
                    l1.time[index_l1 * blk_l1 + j]++;
            }
            l1.time[index_l1 * blk_l1 + i] = 1;
            if (only_l1 == 1)
                l1.dirty[index_l1 * blk_l1 + i] = '1';
            break;
        }
    }
    if (i == blk_l1)
    {
        ptr = evict_l1(addr, tag_l1, index_l1, blk_l1);
        for (int j = 0; j < bl_size; j++)
        {
            if (only_l1 == 0)
                *(l1.data + (blk_l1 * index_l1 + ptr) * bl_size + j) = d[addr + j];
            else
                *(l1.data + (blk_l1 * index_l1 + ptr) * bl_size + j) = *(ptr_l2 + j);
        }
        for (int j = 0; j < blk_l1; j++)
        {
            if (l1.time[index_l1 * blk_l1 + j] < l1.time[index_l1 * blk_l1 + ptr] && l1.time[index_l1 * blk_l1 + j] != 0)
                l1.time[index_l1 * blk_l1 + j]++;
        }
        l1.time[index_l1 * blk_l1 + ptr] = 1;
        l1.tag[index_l1 * blk_l1 + ptr] = tag_l1;
        if (only_l1 == 1)
            l1.dirty[index_l1 * blk_l1 + ptr] = '1';
    }
    if (only_l1 == 1)
        return;

    for (i = 0; i < blk_l2; i++)
    {
        if (l2.tag[index_l2 * blk_l2 + i] == -1)
        {
            l2.tag[index_l2 * blk_l2 + i] = tag_l2;
            for (int j = 0; j < bl_size; j++)
            {
                *(l2.data + (blk_l2 * index_l2 + i) * bl_size + j) = d[addr + j];
            }
            for (int j = 0; j < blk_l2; j++)
            {
                if (l2.time[index_l2 * blk_l2 + j] != 0)
                    l2.time[index_l2 * blk_l2 + j]++;
            }
            l2.time[index_l2 * blk_l2 + i] = 1;
            break;
        }
    }
    if (i == blk_l2)
    {
        ptr = evict_l2(tag_l2, index_l2, blk_l2);
        for (int j = 0; j < bl_size; j++)
        {
            *(l2.data + (blk_l2 * index_l2 + ptr) * bl_size + j) = d[addr + j];
        }
        for (int j = 0; j < blk_l2; j++)
        {
            if (l2.time[index_l2 * blk_l2 + j] < l2.time[index_l2 * blk_l2 + ptr] && l2.time[index_l2 * blk_l2 + j] != 0)
                l2.time[index_l2 * blk_l2 + j]++;
        }
        l2.time[index_l2 * blk_l2 + ptr] = 1;
        l2.tag[index_l2 * blk_l2 + ptr] = tag_l2;
    }
}
void flush_l1()
{
    int blk_l1 = l1.block / l1.set;
    for (int i = 0; i < l1.set; i++)
    {
        for (int j = 0; j < blk_l1; j++)
        {
            if (l1.dirty[i * blk_l1 + j] == '1')
            {
                int addr = l1.tag[i * blk_l1 + j] * power(2, l1.noffset + l1.nindex) + i * power(2, l1.noffset);
                char *ptr_l2 = l2.find_in_cache(addr, 1);
                if (ptr_l2 != NULL)
                {
                    for (int k = 0; k < bl_size; k++)
                        *(ptr_l2 + k) = *(l1.data + (i * blk_l1 + j) * bl_size + k);
                }
                else
                {
                    for (int k = 0; k < bl_size; k++)
                        d[addr + k] = *(l1.data + (i * blk_l1 + j) * bl_size + k);
                }
            }
        }
    }
}
void flush_l2()
{
    int blk_l2 = l2.block / l2.set;
    for (int i = 0; i < l2.set; i++)
    {
        for (int j = 0; j < blk_l2; j++)
        {
            if (l2.dirty[i * blk_l2 + j] == '1')
            {
                int addr = l2.tag[i * blk_l2 + j] * power(2, l2.noffset + l2.nindex) + i * power(2, l2.noffset);

                for (int k = 0; k < bl_size; k++)
                    d[addr + k] = *(l2.data + (i * blk_l2 + j) * bl_size + k);
            }
        }
    }
}

class STALL
{
public:
    int stall, flag;
    STALL()
    {
        stall = 0;
        flag = 0;
    }
} IF, ID, EX, ME, WB;
class instr
{
public:
    int n, type, reg[3], op;
    string label, jlabel;
    int imm;
    instr()
    {
        reg[0] = reg[1] = reg[2] = -1;
    }
    void disp()
    {
        std::cout << n << " " << type << " " << op;
        std::cout << "\nreg:" << reg[0] << " " << reg[1] << " " << reg[2];
        std::cout << "\nlabels:" << label << " " << jlabel << " " << imm << endl;
    }
} I[50];
class latch
{
public:
    int ino, result;
    int s;
    latch()
    {
        ino = -1;
        result = 0;
        s = 0;
    }
} _if, if_id, id_ex, ex_mem, mem_wb;

int findinmap(unordered_map<string, int> m, string key)
{
    for (auto i : m)
        if (i.first == key)
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
    for (int i = 0; i < n; i++)
        if (I[i].jlabel != "\0")
        {
            auto pos = findinmap(labelmap, I[i].jlabel);
            if (pos)
                I[i].imm = labelmap[I[i].jlabel] - I[i].n;
            else
                return 0;
        }
    return 1;
}

int regcheck(string reg)
{
    if (reg.length() < 2 || reg.length() > 3)
        return -1;
    if (reg[0] == 'r' || reg[0] == 'R')
    {
        int r = stoi(reg.substr(1, reg.length() - 1));
        if (r >= 0 && r <= 31)
            return r;
        else
            return -1;
    }
    return -1;
}

int fill(int n, int ptr)
{
    //cout<<endl<<"n"<<n<<"ptr"<<ptr;
    if (ptr + 4 > 4096)
        return 0;
    char *a = (char *)&n;
    for (int i = 0; i < 4; ++i)
    {
        d[ptr + i] = *a;
        a++;
    }
    return 1;
}

int get(int ptr)
{

    int n;
    char *a = (char *)&n;
    for (int i = 0; i <= 3; i++)
    {
        *a = d[ptr + i];
        a++;
    }
    return n;
}

int numcheck(string temp)
{
    for (int p = 0; p < temp.length(); p++)
        if (!(temp[p] >= '0' && temp[p] <= '9' || p == 0 && (temp[p] == '+' || temp[p] == '-')))
            return 0;
    return 1;
}

int numcheckhex(string temp)
{
    for (int p = 0; p < temp.length(); p++)
        if (!(temp[p] >= '0' && temp[p] <= '9' || temp[p] >= 'a' && temp[p] <= 'f'))
            return 0;
    return 1;
}

int parse(string str, int c)
{
    int i = 0, j, k;
    I[c].n = c;
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
        auto iter = find(result.begin(), result.end(), "\0");
        if (iter != result.end())
            result.erase(iter);
        else
            break;
    } while (1);

    char ch = result[0][result[0].length() - 1];
    if (ch == ':')
    {
        I[c].label = result[0].substr(0, result[0].length() - 1);
        labelmap[I[c].label] = c;
        if (result.size() == 1)
        {
            I[c].type = -1;
            return 1;
        }
        i++;
    }
    if (result[0][0] == '#')
    {
        I[c].type = -1;
        return 1;
    }
    else if (ch == ':' && result.size() > 1 && result[1][0] == '#')
    {
        I[c].type = -1;
        return 1;
    }

    for (j = 0; j < 6; j++)
    {
        for (k = 0; k < t[j].size(); k++)
            if (result[i] == t[j][k])
            {
                I[c].type = j;
                I[c].op = k;
                break;
            }
        if (k < t[j].size())
            break;
    }
    if (j == 6 || result.size() < tn[j])
        return 0;

    i++;
    int p;
    vector<string> temp;
    switch (I[c].type)
    {
    case 0:
    case 1:
    case 3:
        for (p = i; p < i + 2; p++)
        {
            int r = regcheck(result[p]);
            if (r == -1)
                return 0;
            I[c].reg[p - i] = r;
        }
        i += 2;
        break;
    case 2:
        I[c].reg[0] = regcheck(result[i]);
        if (I[c].reg[0] == -1)
            return 0;
        boost::algorithm::split(temp, result[i + 1], is_any_of(" ()"));
        if (numcheck(temp[0]) == 1)
            I[c].imm = stoi(temp[0]);
        else
            return 0;
        I[c].reg[1] = regcheck(temp[1]);
        if (I[c].reg[1] == -1 || temp.size() != 3)
            return 0;
        i += 2;
        //if(i!=result.size()) return 0;
        break;
    case 4:
        I[c].jlabel = result[i];
        i++;
        //if(i!=result.size()) return 0;
        break;
    case 5:
        I[c].reg[0] = regcheck(result[i]);
        if (I[c].reg[0] == -1)
            return 0;
        i++;
        if (I[c].op == 0)
        {
            if (dataseg.find(result[i]) != dataseg.end())
            {
                I[c].imm = dataseg[result[i]];
                i++;
            }
            else
                return 0;
            //if(i!=result.size()) return 0;
        }
        break;
    }
    if (I[c].type == 0)
    {
        I[c].reg[2] = regcheck(result[i]);
        if (I[c].reg[2] == -1)
            return 0;
        i++;
    }
    else if (I[c].type == 1 || I[c].type == 5 && I[c].op == 1)
    {
        if (numcheck(result[i]) == 1)
            I[c].imm = stoi(result[i]);
        else if (result[i].substr(0, 2) == "0x" || result[i].substr(0, 2) == "0X")
        {
            string fun = result[i].substr(2, result[i].length() - 2);
            if (numcheckhex(fun))
                I[c].imm = strtoul(result[i].c_str(), 0, 16);
            else
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
        i++;
    }
    else if (I[c].type == 3)
    {
        I[c].jlabel = result[i];
        i++;
    }
    if (ch == ':' && i == tn[j] + 1)
        return 1;
    else if (i == tn[j])
        return 1;
    else if (result[i][0] == '#')
        return 1;
    else
        return 0;
}

int execute(int start, int n)
{

    int c = start, addr;
    do
    {
        switch (I[c].type)
        {
        case 0:
            switch (I[c].op)
            {
            case 0:
                r[I[c].reg[0]] = r[I[c].reg[1]] + r[I[c].reg[2]];
                break;
            case 1:
                r[I[c].reg[0]] = r[I[c].reg[1]] - r[I[c].reg[2]];
                break;
            case 2:
                if (r[I[c].reg[1]] < r[I[c].reg[2]])
                    r[I[c].reg[0]] = 1;
                else
                    r[I[c].reg[0]] = 0;
                break;
            }
            c++;
            break;
        case 1:
            switch (I[c].op)
            {
            case 0:
                r[I[c].reg[0]] = r[I[c].reg[1]] + I[c].imm;
                break;
            case 1:
                if (I[c].imm >= 0)
                    r[I[c].reg[0]] = r[I[c].reg[1]] << I[c].imm;
                else
                    r[I[c].reg[0]] = r[I[c].reg[1]] >> (-I[c].imm);
                break;
            }
            c++;
            break;
        case 2:
            addr = r[I[c].reg[1]] + I[c].imm;
            switch (I[c].op)
            {
            case 0:
                if (addr + 4 <= 4096)
                    r[I[c].reg[0]] = get(addr);
                else
                {
                    cout << c;
                    return 0;
                }
                break;
            case 1:
                if (fill(r[I[c].reg[0]], addr) == 0)
                {
                    cout << c;
                    return 0;
                }
                break;
            }
            c++;
            break;
        case 3:
            switch (I[c].op)
            {
            case 0:
                if (r[I[c].reg[0]] != r[I[c].reg[1]])
                    c += I[c].imm;
                else
                    c++;
                break;
            case 1:
                if (r[I[c].reg[0]] == r[I[c].reg[1]])
                    c += I[c].imm;
                else
                    c++;
                break;
            }
            break;
        case 4:
            switch (I[c].op)
            {
            case 0:
                c += I[c].imm;
                break;
            }
            break;
        case 5:
            switch (I[c].op)
            {
            case 0:
            case 1:
                r[I[c].reg[0]] = I[c].imm;
                c++;
                break;
            }
            break;
        case -1:
            c++;
            break;
        }
        if (c - start == n)
            break;
    } while (1);
    return 1;
}
void insf(int n)
{ //cout<<"if";
    if (_if.ino >= n)
    {
        if (_if.s == 0)
            if_id = _if;
        return;
    }
    if (_if.s == 1)
    {
        if_id.s = 1; //cout<<"hi";
    }
    else
    {
        total_instr++;
        if_id = _if;
        //cout<<_if.ino<<" ";
        if (I[_if.ino].type == 3)
        {
            //cout<<"stall put";
            stallcount++;
            IF.stall += 2;
            IF.flag = 1;
            _if.s = 1;
            //if_id.s=1;
        }
        else if (I[_if.ino].type == 4)
        {
            stallcount += 2;
            IF.stall += 3;
            IF.flag = 1;
            _if.s = 1;
            //if_id.s=1;
        }
        pc++;
    }
}
int flag1 = 0;
void idrf(int n)
{ //cout<<"id";
    if (if_id.ino >= n)
    {
        if (if_id.s == 0)
            id_ex = if_id;
        return;
    }
    if (if_id.ino == -1)
        return;
    if (if_id.s == 1)
    {
        id_ex.s = 1;
    }
    else
    {
        //cout<<"ID STALL"<<ID.stall;
        int c = if_id.ino;
        //cout<<c<<" ";
        if (I[c].type == 3)
        {
            int cex = ex_mem.ino;
            //cout<<"cex"<<cex;
            int reg0 = r[I[c].reg[0]], reg1 = r[I[c].reg[1]];
            if (cex != -1 && (I[c].reg[0] == I[cex].reg[0] || I[c].reg[1] == I[cex].reg[0]) && I[cex].type == 2 && I[cex].op == 0)
            {
                if (ID.stall == 0 && flag1 == 1)
                {
                    if (I[c].reg[0] == I[cex].reg[0])
                        reg0 = mem_wb.result;
                    if (I[c].reg[1] == I[cex].reg[0])
                        reg1 = mem_wb.result;
                    flag1 = 0;
                }
                else if (ID.stall == 0 && flag1 == 0)
                {
                    //cout<<"ID STALL";
                    stallcount += 2;
                    ID.stall += 2;
                    ID.flag = 1;
                    if_id.s = 1;
                    id_ex.s = 1;
                    flag1 = 1;
                    return;
                }
            }
            else if (cex != -1 && (I[c].reg[0] == I[cex].reg[0] || I[c].reg[1] == I[cex].reg[0]) && (I[cex].type == 0 || I[cex].type == 1 || I[cex].type == 5))
            {
                if (ID.stall == 0 && flag1 == 1)
                {
                    //cout<<"ID STALL 0";
                    if (I[c].reg[0] == I[cex].reg[0])
                        reg0 = ex_mem.result;
                    if (I[c].reg[1] == I[cex].reg[0])
                        reg1 = ex_mem.result;
                    flag1 = 0;
                }
                else if (ID.stall == 0 && flag1 == 0)
                {
                    //cout<<"ID STALL 1";
                    stallcount += 1;
                    ID.stall += 1;
                    ID.flag = 1;
                    id_ex.s = 1;
                    if_id.s = 1;
                    flag1 = 1;
                    return;
                }
            }
            switch (I[c].op)
            {
            case 0:
                if (reg0 != reg1)
                    pc += I[c].imm - 1;
                break;
            case 1:
                if (reg0 == reg1)
                    pc += I[c].imm - 1;
                break;
            }
        }
        id_ex = if_id;
    }
}

int ex(int n)
{ //cout<<"ex";
    if (id_ex.ino >= n)
    {
        if (id_ex.s == 0)
            ex_mem = id_ex;
        return 0;
    }
    if (id_ex.ino == -1)
        return 0;
    if (id_ex.s == 1)
        ex_mem.s = 1;
    else
    {
        int c = id_ex.ino;
        //cout<<c<<" ";
        int addr;
        int cex = ex_mem.ino;
        int cmem = mem_wb.ino;
        int reg1 = r[I[c].reg[1]], reg2 = r[I[c].reg[2]];
        if (cex != -1 && I[c].reg[1] == I[cex].reg[0] && (I[cex].type == 0 || I[cex].type == 1 || I[cex].type == 5))
        {
            reg1 = ex_mem.result;
            //cout << "r" << I[c].reg[1] << ":" << reg1 << " ";
        }
        if (cex != -1 && I[c].reg[2] == I[cex].reg[0] && (I[cex].type == 0 || I[cex].type == 1 || I[cex].type == 5))
        {
            reg2 = ex_mem.result;
            //cout << "r" << I[c].reg[2] << ":" << reg2 << " ";
        }
        if (cmem != -1 && (I[c].reg[1] == I[cmem].reg[0] || I[c].reg[2] == I[cmem].reg[0]) && I[cmem].type == 2 && I[cmem].op == 0)
        {
            if (flag == 1)
            {
                if (I[c].reg[1] == I[cmem].reg[0])
                    reg1 = mem_wb.result;
                if (I[c].reg[2] == I[cmem].reg[0])
                    reg2 = mem_wb.result;
                flag = 0;
            }
            else
            {
                if (!(I[c].reg[2] == I[cmem].reg[0] && I[c].type == 2))
                {
                    stallcount++;
                    EX.stall++;
                    EX.flag = 1;
                    flag = 1;
                    id_ex.s = 1;
                    ex_mem.s = 1;
                    return 0;
                }
            }
        }
        ex_mem = id_ex;
        switch (I[c].type)
        {
        case 0:
            switch (I[c].op)
            {
            case 0:
                ex_mem.result = reg1 + reg2;
                break;
            case 1:
                ex_mem.result = reg1 - reg2;
                break;
            case 2:
                if (reg1 < reg2)
                    ex_mem.result = 1;
                else
                    ex_mem.result = 0;
                break;
            }
            break;
        case 1:
            switch (I[c].op)
            {
            case 0:
                ex_mem.result = reg1 + I[c].imm;
                break;
            case 1:
                if (I[c].imm >= 0)
                    ex_mem.result = reg1 << I[c].imm;
                else
                    ex_mem.result = reg1 >> (-I[c].imm);
                break;
            }
            break;
        case 2:
            addr = reg1 + I[c].imm;
            if (addr + 4 > 4096)
            {
                return -1;
            }
            else
                ex_mem.result = addr;
            break;
        case 4:
            switch (I[c].op)
            {
            case 0:
                pc += I[c].imm - 1;
                break;
            }
            break;
        case 5:
            switch (I[c].op)
            {
            case 0:
            case 1:
                ex_mem.result = I[c].imm;
                break;
            }
            break;
        }
    }
    return 0;
}
void mem(int n)
{
    //cout<<"me";
    if (ex_mem.ino >= n)
    {
        if (ex_mem.s == 0)
            mem_wb = ex_mem;
        return;
    }
    if (ex_mem.ino == -1)
        return;
    if (ex_mem.s == 1)
        mem_wb.s = 1;
    else
    {
        int c = ex_mem.ino, addr, reg0 = r[I[c].reg[0]];
        int cmem = mem_wb.ino, result = mem_wb.result;
        int f = 0;
        int n;
        char *a = (char *)&n;
        char *ptr;
        //cout<<c<<" ";
        if (I[c].type == 2)
            switch (I[c].op)
            {
            case 0:
                total_mem_acc++;
                addr = ex_mem.result;
                ME.flag = 1;
                stallcount += l1.stall - 1;
                ME.stall += l1.stall - 1;
                ptr = l1.find_in_cache(addr, 0);
                if (ptr != NULL)
                {
                    int offset = addr % power(2, l1.noffset);
                    for (int i = 0; i <= 3; i++)
                    {
                        if (offset + i < bl_size)
                        {
                            *a = *(ptr + offset + i);
                            a++;
                        }
                        else
                        {
                            *a = d[addr + i];
                            a++;
                            bl_spill++;
                        }
                    }
                }
                else
                {
                    l1_miss++;
                    ME.stall += l2.stall;
                    stallcount += l2.stall;
                    ptr = l2.find_in_cache(addr, 0);
                    if (ptr != NULL)
                    {
                        int offset = addr % power(2, l2.noffset);
                        for (int i = 0; i <= 3; i++)
                        {
                            if (offset + i < bl_size)
                            {
                                *a = *(ptr + offset + i);
                                a++;
                            }
                            else
                            {
                                *a = d[addr + i];
                                a++;
                                bl_spill++;
                            }
                        }
                        insert(addr, 1, ptr);
                    }
                    else
                    {
                        l2_miss++;
                        ME.stall += mem_stall;
                        stallcount += mem_stall;
                        for (int i = 0; i <= 3; i++)
                        {
                            *a = d[addr + i];
                            a++;
                        }
                        insert(addr);
                    }
                }

                mem_wb.result = n;
                f = 1;
                break;
            case 1:
                if (cmem != -1 && I[c].reg[0] == I[cmem].reg[0] && (I[cmem].type == 0 || I[cmem].type == 1 || I[cmem].type == 5 || (I[cmem].type == 2 && I[cmem].op == 0)))
                    reg0 = result;
                addr = ex_mem.result;
                a = (char *)&reg0;
                ME.flag = 1;
                total_mem_acc++;
                ME.stall += l1.stall - 1;
                stallcount += l1.stall - 1;
                ptr = l1.find_in_cache(addr, 1);
                if (ptr != NULL)
                {
                    int offset = addr % power(2, l1.noffset);

                    for (int i = 0; i <= 3; i++)
                    {
                        if (offset + i < bl_size)
                        {
                            *(ptr + offset + i) = *a;
                            a++;
                        }
                        else
                        {
                            d[addr + i] = *a;
                            a++;
                            bl_spill++;
                        }
                    }
                }
                else
                {
                    l1_miss++;
                    ME.stall += l2.stall;
                    stallcount += l2.stall;
                    ptr = l2.find_in_cache(addr, 1);
                    if (ptr != NULL)
                    {
                        int offset = addr % power(2, l2.noffset);
                        for (int i = 0; i <= 3; i++)
                        {
                            if (offset + i < bl_size)
                            {
                                *(ptr + offset + i) = *a;
                                a++;
                            }
                            else
                            {
                                d[addr + i] = *a;
                                a++;
                                bl_spill++;
                            }
                        }
                        insert(addr, 1, ptr);
                    }
                    else
                    {
                        l2_miss++;
                        ME.stall += mem_stall;
                        stallcount += mem_stall;
                        for (int i = 0; i <= 3; i++)
                        {
                            d[addr + i] = *a;
                            a++;
                        }
                        insert(addr);
                    }
                }
                break;
            }
        mem_wb.ino = ex_mem.ino;
        mem_wb.s = ex_mem.s;
        if (f == 0)
            mem_wb.result = ex_mem.result;
    }
}
void wb(int n)
{
    //cout<<"wb";
    if (mem_wb.ino >= n)
        return;
    if (mem_wb.ino == -1)
        return;
    if (mem_wb.s == 1)
        ;
    else
    {
        int c = mem_wb.ino;
        //cout<<c<<" ";
        if (I[c].type == 0 || I[c].type == 1 || I[c].type == 5 || (I[c].type == 2 && I[c].op == 0))
            r[I[c].reg[0]] = mem_wb.result;
    }
}

int execstep(int c)
{
    int addr;
    switch (I[c].type)
    {
    case 0:
        switch (I[c].op)
        {
        case 0:
            r[I[c].reg[0]] = r[I[c].reg[1]] + r[I[c].reg[2]];
            break;
        case 1:
            r[I[c].reg[0]] = r[I[c].reg[1]] - r[I[c].reg[2]];
            break;
        case 2:
            if (r[I[c].reg[1]] < r[I[c].reg[2]])
                r[I[c].reg[0]] = 1;
            else
                r[I[c].reg[0]] = 0;
            break;
        }
        c++;
        break;
    case 1:
        switch (I[c].op)
        {
        case 0:
            r[I[c].reg[0]] = r[I[c].reg[1]] + I[c].imm;
            break;
        case 1:
            if (I[c].imm >= 0)
                r[I[c].reg[0]] = r[I[c].reg[1]] << I[c].imm;
            else
                r[I[c].reg[0]] = r[I[c].reg[1]] >> (-I[c].imm);
            break;
        }
        c++;
        break;
    case 2:
        addr = r[I[c].reg[1]] + I[c].imm;
        switch (I[c].op)
        {
        case 0:
            if (addr + 4 <= 4096)
                r[I[c].reg[0]] = get(addr);
            else
            {
                cout << c;
                return -1;
            }
            break;
        case 1:
            if (fill(r[I[c].reg[0]], addr) == 0)
            {
                cout << c;
                return -1;
            }
            break;
        }
        c++;
        break;
    case 3:
        switch (I[c].op)
        {
        case 0:
            if (r[I[c].reg[0]] != r[I[c].reg[1]])
                c += I[c].imm;
            else
                c++;
            break;
        case 1:
            if (r[I[c].reg[0]] == r[I[c].reg[1]])
                c += I[c].imm;
            else
                c++;
            break;
        }
        break;
    case 4:
        switch (I[c].op)
        {
        case 0:
            c += I[c].imm;
            break;
        }
        break;
    case 5:
        switch (I[c].op)
        {
        case 0:
        case 1:
            r[I[c].reg[0]] = I[c].imm;
            c++;
            break;
        }
        break;
    case -1:
        c++;
        break;
    }
    return c;
}
int maincheck()
{
    auto itr = findinmap(labelmap, "main");
    if (itr)
        return labelmap["main"];
    else
        return -1;
}

int main()
{
    float normal_cycles, IPC;
    cout << "Enter .asm filename(with extension):";
    string s;
    cin >> s;
    ifstream f(s);
    if (!f)
    {
        cout << "File not found";
        return 0;
    }
    string str;
    do
    {
        std::getline(f, str);
    } while (str == " " || str == "\t" || str == "\0" || str == "\n" || str[0] == '#');
    /*if(str!=".data")
		{
			std::cout<<"where s data segment";
			return 0;
		}*/
    vector<string> tmp;
    boost::algorithm::split(tmp, str, is_any_of(" \t"));

    if (tmp.size() == 1 && tmp[0] != ".data" && tmp[0] != ".text")
    {
        std::cout << "\nError no data/text segment";
        return 0;
    }
    else if (tmp.size() > 1 && tmp[1][0] != '#')
    {
        cout << "Error";
        return 0;
    }
    if (tmp[0] == ".text")
        goto label;
    do
    {
        std::getline(f, str);
        if (str == "\0" || str[0] == '#')
            continue;
        vector<string> temp;
        boost::algorithm::split(temp, str, is_any_of(" \t:"));
        do
        {
            auto iter = find(temp.begin(), temp.end(), "\0");
            if (iter == temp.end())
                break;
            temp.erase(iter);
        } while (1);
        if (temp.size() > 1 && temp[1][0] != '#')
        {
            std::cout << "\nError";
            return 0;
        }
        if (temp[0] == ".text")
        {
            break;
        }
        do
        {
            std::getline(f, str);
        } while (str == "\0" || str == "\n" || str[0] == '#');
        vector<string> temp1;
        boost::algorithm::split(temp1, str, is_any_of(" \t"));
        do
        {
            auto iter = find(temp1.begin(), temp1.end(), "\0");
            if (iter != temp1.end())
                temp1.erase(iter);
            else
                break;
        } while (1);
        int v = 0;
        if (temp1[0] == ".word")
        {
            for (int k = 1; k < temp1.size(); ++k)
            {
                if (numcheck(temp1[k]))
                {
                    //std::cout<<temp1[k]<<" ";
                    fill(stoi(temp1[k]), point + v);
                    v += 4;
                }
                else if (temp1[k][0] != '#')
                {
                    cout << "\nError";
                    return 0;
                }
            }
        }
        else
        {
            cout << "\nError";
            return 0;
        }
        dataseg[temp[0]] = point;
        point += v;

    } while (1);
label:
    int c = 0;
    while (std::getline(f, str))
    {
        if (str == "\0" || str == "\n" || str[0] == '#')
            continue;
        int p = parse(str, c);
        //I[c].disp();
        if (p == 0)
        {
            std::cout << "\nError on inst " << c;
            return 0;
        }

        c++;
    }

    /*for(int i=0;i<5;i++)
		std::cout<<get(4*i)<<" ";*/
    int start = maincheck();
    if (start == -1)
    {
        std::cout << "\nError No main";
        return 0;
    }
    if (labelcheck(c) == 0)
    {
        std::cout << "\nError Labels not matched";
        return 0;
    }

    //  int l1_size, l2_size, bl_size, sets, assoc;
    //  int l1_stall, l2_stall, mem_stall;
    f.close();
    string S;
    int cache_details[8] = {0};
    int counter = 0;
    cout << "\nEnter cache details filename with extension: ";
    cin >> S;
    f.open(S);
    if (!f)
    {
        cout << "File not found";
        return 0;
    }
    do
    {
        string str1;
        std::getline(f, str1);
        vector<string> temp;
        boost::algorithm::split(temp, str1, is_any_of(" \t:"));
        for (int b = 0; b < temp.size(); b++)
        {
            int p;
            for (p = 0; p < temp[b].length(); p++)
                if (!(temp[b][p] >= '0' && temp[b][p] <= '9'))
                    break;
            if (p == temp[b].length() && p != 0)
            {
                //cout << temp[b] << endl;
                cache_details[counter] = stoi(temp[b]);
                counter++;
            }
        }
    } while (f);
    f.close();
    //cout << "l1_size:";
    l1.size = cache_details[0];
    //cout << "l2_size:";
    l2.size = cache_details[1];
    //cout << "block_size:";
    bl_size = cache_details[2];
    //cout << "l1_associativity:";
    l1.assoc = cache_details[3];
    //cout << "l2_associativity:";
    l2.assoc = cache_details[4];
    //cout << "l1_latency:";
    l1.stall = cache_details[5];
    //cout << "l2_latency:";
    l2.stall = cache_details[6];
    //cout << "main memory latency:";
    mem_stall = cache_details[7];
    l1.block = l1.size / bl_size;
    l2.block = l2.size / bl_size;
    l1.set = 1;
    if (l1.assoc != 0)
        l1.set = l1.block / l1.assoc;
    l2.set = 1;
    if (l2.assoc != 0)
        l2.set = l2.block / l2.assoc;

    l1.tag = new int[l1.block];
    l2.tag = new int[l2.block];
    l1.time = new int[l1.block];
    l2.time = new int[l2.block];
    l1.dirty = new char[l1.block];
    l2.dirty = new char[l2.block];
    for (int d = 0; d < l2.block; d++)
    {
        l2.tag[d] = -1;
        l2.time[d] = 0;
        l2.dirty[d] = '0';
    }
    for (int d = 0; d < l1.block; d++)
    {
        l1.tag[d] = -1;
        l1.time[d] = 0;
        l1.dirty[d] = '0';
    }
    l1.data = new char[l1.size];
    l2.data = new char[l2.size];

    l1.noffset = (int)log2(bl_size);
    l1.nindex = (int)log2(l1.set);
    l1.ntag = 12 - l1.noffset - l1.nindex;

    l2.noffset = (int)log2(bl_size);
    l2.nindex = (int)log2(l2.set);
    l2.ntag = 12 - l2.noffset - l2.nindex;

    cout << "\n1.Run\t2.Run step by step\t3.Run with pipelining\nYour choice:";
    int choice;
    int cur = 0, check, key;
    cin >> choice;
    switch (choice)
    {
    case 1:
        check = execute(start, c);
        if (check == 0)
            std::cout << "\nError";
        else
        {
            for (int i = 0; i < 32; i++)
            {
                std::cout << "r" << i << "=" << r[i] << "\t";
                if ((i + 1) % 4 == 0)
                    cout << endl;
            }
            cout << "\nData segment:" << endl;
            for (int i = 0; i < point; i += 4)
                cout << i << ": " << get(i) << endl;
            if (point == 0)
                cout << 0 << endl;
        }
        break;
    case 2:
        cout << "\nPress Enter to move forward a step and q to quit\n";
        while (cur != c)
        {
            key = cin.get();
            if (key == 'q')
                break;
            int temp = cur;

            cur = execstep(cur);
            if (cur == -1)
            {
                cout << "\nError on inst. " << temp;
                break;
            }
            cout << "\nExecuted inst. " << temp << endl;
            for (int i = 0; i < 32; i++)
            {
                std::cout << "r" << i << "=" << r[i] << "\t";
                if ((i + 1) % 4 == 0)
                    cout << endl;
            }
            cout << "\nData segment:" << endl;
            for (int i = 0; i < point; i += 4)
                cout << i << ": " << get(i) << endl;
            if (point == 0)
                cout << 0 << endl;
        }
        break;
    case 3:
        pc = start;
        _if.ino = pc;
        do
        {
            //cout << endl
            //	 << "pc=" << pc << endl;
            if (IF.stall == 0 && IF.flag == 1)
            {
                _if.s = 0;
                IF.flag = 0;
            }
            if (ID.stall == 0 && ID.flag == 1)
            {
                if_id.s = 0;
                ID.flag = 0;
            }
            if (EX.stall == 0 && EX.flag == 1)
            {
                id_ex.s = 0;
                EX.flag = 0;
            }
            if (ME.stall == 0 && ME.flag == 1)
            {
                ex_mem.s = 0;
                ME.flag = 0;
            }
            if (WB.stall == 0 && WB.flag == 1)
            {
                mem_wb.s = 0;
                WB.flag = 0;
            }

            wb(c);
            if (mem_wb.ino == c)
                break;
            if (mem_wb.s == 1 && WB.stall > 0)
            {
                WB.stall--;
                continue;
            }
            mem(c);
            if (ex_mem.s == 1 && ME.stall > 0)
            {
                ME.stall--;
                continue;
            }
            int o = ex(c);
            if (id_ex.s == 1 && EX.stall > 0)
            {
                EX.stall--;
                continue;
            }
            if (o == -1)
            {
                cout << "\nError on inst. " << id_ex.ino;
                break;
            }
            idrf(c);
            if (if_id.s == 1 && ID.stall > 0)
            {
                ID.stall--;
                continue;
            }
            insf(c);
            _if.ino = pc;
            if (_if.s == 1 && IF.stall > 0)
            {
                IF.stall--;
                //cout<<"stall on if";
                continue;
            }

        } while (1);

        for (int i = 0; i < 32; i++)
        {
            std::cout << "r" << i << "=" << r[i] << "\t";
            if ((i + 1) % 4 == 0)
                cout << endl;
        }
        cout << "L1:";
        l1.disp();
        cout << "\n\nL2:";
        l2.disp();
        cout << "\n\nData segment:" << endl;
        for (int i = 0; i < 40; i += 4)
            cout << i << ": " << get(i) << endl;

        cout << "\nNumber of stalls: " << stallcount;
        if (total_mem_acc == 0)
            cout << "\nL1 miss rate: 0\nL2 miss rate: 0";
        else
        {
            float l1_miss_rate = (float)(l1_miss) / total_mem_acc;
            printf("\nL1 miss rate: %.2f", l1_miss_rate);
            if (l1_miss == 0)
                cout << "L2 miss rate: 0";
            else
            {
                float l2_miss_rate = (float)(l2_miss) / l1_miss;
                printf("\nL2 miss rate: %.2f", l2_miss_rate);
            }
        }
        normal_cycles = 5 + (total_instr - 1);
        //cout << total_instr << endl;
        IPC = total_instr / (normal_cycles + stallcount);
        printf("\nIPC: %.3f", IPC);
        //cout << "\nblock spills: " << bl_spill;
        flush_l1();
        flush_l2();
        cout << "\n\nData segment after flushing cache:" << endl;
        for (int i = 0; i < 40; i += 4)
            cout << i << ": " << get(i) << endl;

        break;
    default:
        cout << "\nWrong choice!";
        break;
    }

    //cout<<endl<<"start="<<start<<endl<<"end="<<c;

    return 0;
}
