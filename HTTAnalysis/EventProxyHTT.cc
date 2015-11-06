#include "EventProxyHTT.h"

#include <iostream>

EventProxyHTT::EventProxyHTT(){}
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
EventProxyHTT::~EventProxyHTT(){}
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
void EventProxyHTT::init(std::vector<std::string> const& iFileNames){

	treeName_ = "m2n/newevent";

	EventProxyBase::init(iFileNames);

	fChain->SetMakeClass(0);

	
	///Add weight friend TTree
	bool hasWeightFile = false;
	TChain *fFriendChainWeights = new TChain("Summary/tree");	
	TFile *file = new TFile("RootAnalysis_Weights.root");
	TTree *treeWeights;
	if(file->IsOpen() && file->FindObjectAny("tree")){
	  treeWeights = (TTree*)file->Get("Summary/tree");
	  fChain->AddFriend(treeWeights);
	  hasWeightFile = true;
	}
	//////////	
	puWeight = -1;
	
	wevent = 0;//IMPORTNANT!!
	wpair = 0;//IMPORTNANT!!
	wtau = 0;//IMPORTNANT!!
	wmu = 0;//IMPORTNANT!!
	
	if(hasWeightFile) fChain->SetBranchAddress("PUWeight",&puWeight);
	fChain->SetBranchAddress("wevent",&wevent);
	fChain->SetBranchAddress("wpair",&wpair);
	fChain->SetBranchAddress("wtau",&wtau);
	fChain->SetBranchAddress("wmu",&wmu);

	
	//fChain->SetBranchStatus("*",0);
	fChain->SetBranchStatus("PUWeight",1);
	fChain->SetBranchStatus("sample_",1);
	fChain->SetBranchStatus("genevtweight_",1);
	fChain->SetBranchStatus("npv_",1);
	fChain->SetBranchStatus("wpair",1);
	fChain->SetBranchStatus("wtau",1);
	fChain->SetBranchStatus("wmu",1);
	

}
/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
