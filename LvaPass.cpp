#include <vector>
#include <set>
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/IR/CFG.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/BasicBlock.h"

using namespace llvm;
using namespace std;

namespace{
class Initialize{
	public:
	BitVector in;
	BitVector out;
	Initialize(){}
	Initialize(BitVector inn, BitVector outt){
		in = inn;
		out = outt;
	}
	};
class FlowOrder{
	public:
		DenseMap<BasicBlock*, Initialize> flowOrder;
		FlowOrder(){}
		FlowOrder(DenseMap<BasicBlock*, Initialize> fo){
			flowOrder = fo;
		}
		}; 
class LvaPass: public FunctionPass{
	public:
		static char ID;
		vector<Value*> domain_values;
		BitVector boundary_values, block_values;
		DenseMap<int,StringRef> indexToInst;	
		DenseMap<StringRef,int> indexOfInst;	
		LvaPass(): FunctionPass(ID){}

virtual bool runOnFunction(Function &F){
	domain_values.clear();
	for(auto curr = F.arg_begin(); curr != F.arg_end(); curr++){
		domain_values.push_back(curr);
	}
	for(inst_iterator itr = inst_begin(F); itr!=inst_end(F); itr++){
		Value* x = &*itr;
		if((*itr).hasName())
		domain_values.push_back(x);
	}
	int flag = 0;
	int size = domain_values.size();
	for(int x = 0; x<domain_values.size(); x++){
		indexOfInst[domain_values[x]->getName()] = x;
		indexToInst[x] = domain_values[x]->getName();
	}

	BitVector boundary_val(size, false);

	for(auto x : domain_values){
		if(isa<Argument>(x)){
			boundary_val[flag] = 1;
		}
	flag++;
	}

	BitVector block_val(size, false);
	block_values = block_val;
	boundary_values = boundary_val;
	FlowOrder f = run(F);
	//First output of assignment
	errs()<<"Live values out of each Basic Block :\n";
	errs()<<"--------------------------------------\n";
	errs()<<"\tBasic Block \t:\t Live Values\n";
	errs()<<"\t-------------------------------------\n";
	for(auto i = f.flowOrder.begin(); i!=f.flowOrder.end(); i++){
		errs()<<"\t" << (*(*i).first).getName() << "\t\t:\t";
		BitVector bv = (*i).second.out;
		for(int i=0; i<domain_values.size(); i++){
			if(bv[i]){
			errs()<<indexToInst[i] << ',';
			}
		}
		errs()<<"\b \n";
	}
	
	DenseMap<BasicBlock*, vector<pair<int,set<StringRef>>>> liveVarPos;
	for(auto i = f.flowOrder.begin(); i!=f.flowOrder.end(); ++i){
		int pgmPt = (*i).first->size() + 1;
		set<StringRef> live_var;
		BitVector bv = (*i).second.out;
		for(int j=0; j<domain_values.size(); ++j){
			if(bv[j]){
				live_var.insert(indexToInst[j]);
			}
		}

	pgmPt--;
	pair<int,set<StringRef>> tmp(pgmPt,live_var);
	liveVarPos[(*i).first].push_back(tmp);
	for(auto index=(*i).first->rbegin(); index!=(*i).first->rend(); ++index){
		if((*index).getOpcode() != Instruction::PHI){
			if((*index).hasName() && live_var.find((*index).getName())!=live_var.end())
				live_var.erase((*index).getName());

		for(int j=0; j<(*index).getNumOperands(); ++j){
			if((*index).getOperand(j)->hasName() && indexOfInst.find((*index).getOperand(j)->getName())!=indexOfInst.end()){
				live_var.insert((*index).getOperand(j)->getName());
			}
		}

		pgmPt--;
		tmp.first = pgmPt;
		tmp.second = live_var;
		liveVarPos[(*i).first].push_back(tmp);
		}
		else{
			pgmPt--;
			tmp.first = pgmPt;
			set<StringRef> a;
			tmp.second = a;
			liveVarPos[(*i).first].push_back(tmp);
		}
	}
	}

	map<int,int> histogram;//map for plotting histogram
	//second output of assignment
	errs()<<"----------------------------------------------------\n\n";
	errs()<<"Live values at each program point in each Basic Block : \n";
	errs()<<"----------------------------------------------------\n";
	for(auto x=liveVarPos.begin(); x!=liveVarPos.end(); ++x){
		errs()<<" "<<(*x).first->getName() << " :\n";
		errs()<<"\tprogram_point : live values\n";
		errs()<<"\t----------------------------\n";
		for(auto y : (*x).second){
			errs()<<"\t" << y.first << "\t     " << " : ";
			histogram[y.second.size()]++;
			for(auto z : y.second){
				errs()<< " " << z << ',';
			}
			errs()<< "\b \n";
		}
	}
	//third output of assignment
	errs()<< "\n------------------------------------------------------------------\n\n";
	errs()<< "Histogram : \n";
	errs()<< "------------\n";
	errs()<< "#live_values\t: #program_points\n";
	errs()<< "---------------------------------\n";
	for(auto x=histogram.begin(); x!=histogram.end(); x++){
		errs()<< "\t" << (*x).first << "\t:\t" << (*x).second << '\n';
	}

	return false;
}
FlowOrder run(Function &F){
	DenseMap<BasicBlock*,BitVector> kill;
	DenseMap<BasicBlock*,BitVector> gen;
	BasicBlock* firstBB = &F.front();
	DenseMap<BasicBlock*, Initialize> bbFlowValues;
	for(Function::iterator bb = F.begin(); bb != F.end(); bb++){
		if(&*bb != &*firstBB){
			Initialize tmp(block_values, block_values);
			bbFlowValues[&*bb] = tmp;
		}
		else{
			Initialize tmp(boundary_values, block_values);
			bbFlowValues[&*bb] = tmp;
		}
	}

	for(Function::iterator bb = F.begin(); bb != F.end(); ++bb){
		BitVector kill2(domain_values.size(),false);
		BitVector gen2(domain_values.size(),false);

	for(auto &x : *bb){
		if(&*bb == &*firstBB){
			for(auto y=F.arg_begin(); y!=F.arg_end(); ++y){
				kill2.set(indexOfInst[(*y).getName()]);
			}
		}
		for(auto z=0; z<x.getNumOperands(); z++){
			if(x.getOperand(z)->hasName() && indexOfInst.find(x.getOperand(z)->getName())!=indexOfInst.end()){
				if(!kill2[indexOfInst[x.getOperand(z)->getName()]])
					gen2.set(indexOfInst[x.getOperand(z)->getName()]);
			}
		}
		if(x.hasName())
			kill2.set(indexOfInst[x.getName()]);
	}
	gen[&*bb] = gen2;
	kill[&*bb] = kill2;
	}
	//dealing with phi nodes
	bool change = false;
	while(!change){
		change = true;
		auto &bbInCFG = F.getBasicBlockList();
		for(auto k=bbInCFG.rbegin(); k!=bbInCFG.rend(); k++){
			BitVector curr_in(domain_values.size(),false);
			BitVector curr_out(domain_values.size(),false);
			BitVector out2(domain_values.size(),false);
			curr_in = bbFlowValues[&*k].in;
			curr_out = bbFlowValues[&*k].out;
			for(auto k_succ = succ_begin(&*k); k_succ!=succ_end(&*k); k_succ++){
				BitVector in2 = bbFlowValues[&**k_succ].in;
				for(auto inst=(*k_succ)->begin(); inst!=(*k_succ)->end(); inst++){
					if((*inst).getOpcode() == Instruction::PHI){
						for(int i1=0; i1<(*inst).getNumOperands(); ++i1){
							if((*inst).getOperand(i1)->hasName()){
								auto phi = dyn_cast<PHINode>(inst);
								if((*k).getName().compare(phi->getIncomingBlock(i1)->getName())){
									in2[indexOfInst[(*inst).getOperand(i1)->getName()]] = 0;
								}
							}
						}
					}
				}
				out2 |= in2;
			}

			BitVector z(kill[&*k]);
			z.flip();				
			z &= out2;			
			z |= gen[&*k];		
			BitVector in2(z);		
			bbFlowValues[&*k].in	= in2;
			bbFlowValues[&*k].out	= out2;
			if((curr_in != in2 || curr_out != out2) && change)
				change = false;
		}
	}
	FlowOrder flowOrder(bbFlowValues);
	return flowOrder;
}
};
}

char LvaPass::ID = 0;
static RegisterPass<LvaPass> X("LvaPass","Live Variable Analysis");
