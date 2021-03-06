
#include	"begfe.h"
#include	"tool.h"

double 		conditionProb (int inode, int c, Tree *tree, int igene);
void 		InitialParam (Tree *tree);
double		LikelihoodBD (int s, int c, double beta, double brlens);
int         Loglike1tree (Tree *tree, int igene, double *loglike);
int 		Loglike (Tree *tree, double *loglike);
int 		McmcRun (Tree *tree);
int 		MoveBrlens (Tree *tree);
int 		MoveBeta (Tree *tree);
int 		MoveNode (Tree *tree);
void 		PrintHeader (void);
int         PrintState (int round, FILE *outfile);
int         ReadData (FILE *fin);
int         Simulation (Tree *tree);
int         OptimizeExtendbranch (Tree *tree);

/*local variables*/
Tree 		sptree;
Tree		oldtree;
long int	seed=0;
long int	ngenefamily;
double		curLn;
FILE		*fout;
FILE        *fpredict;
FILE		*fsim;
Chain	 	mcmc;
int         alphalink;
int         sim;	
int         pattern[100]; //need to change
int         prior_brlens = 1;
int         prior_r = 1;
double		lambda = 1.0;
double		mu = 1.0;
double		sigma = 1.0;
double		lambda_r = 1.0;
double		mu_r = 1.0;
double		sigma_r = 1.0;
double      maxloglike=0.0;
int			updatetree = 1;

int main (int argc, char *argv[])
{

    FILE *fin;
	clock_t		previousCPUTime, currentCPUTime;

	PrintHeader();
	previousCPUTime = clock();

	fin = (FILE*)gfopen(argv[1],"r");
	    
	if(ReadData (fin) == ERROR)
	{
		MrBayesPrint ("Problem in ReadData\n");
		exit (-1);
	}
	fclose (fin);	

	InitialParam (&sptree);
	
	if(sim == 1) 
	{
		if(Simulation (&sptree) == ERROR)
		{
			printf("Errors in MCMCRUN\n");
			exit(-1);
		}
	}
	else
	{
		if(McmcRun (&sptree) == ERROR)
		{
			printf("Errors in MCMCRUN\n");
			exit(-1);
		}
	}
  
	if(sim == 0){
		currentCPUTime = clock();
		fprintf(fout, "\n   [time: %lf] \n", (currentCPUTime - previousCPUTime) / (double) CLOCKS_PER_SEC);
	}
	
    /*free memory*/
	if(sim == 0){
		fclose(fout);
		fclose(fpredict);
	}
    
  	return(1);
}

int McmcRun (Tree *tree)
{
	int round;
	double random, p=0.0;
	
	if (updatetree==1) {
		p = 0.5;
	}
    
    //optimize parameters on extended branches
    //OptimizeExtendbranch(&sptree);

	/*initial loglikelihood*/
	Loglike (&sptree, &curLn);
    maxloglike = curLn;
	
	/*pick a move*/
	for(round=1; round<mcmc.numGen+1; round++)
	{
		random = rndu();

		/*currently, we donot move the topology of the tree*/
        if(random < p) MoveNode(tree);
		else if (random < 0.8) MoveBrlens(tree);
		else MoveBeta(tree);

		if(round % mcmc.sampleFreq == 0 || round == 1 || round == mcmc.numGen)
		{
			PrintState (round, fout);            
		}
	}	
	return (NO_ERROR);
}
 
int OptimizeExtendbranch (Tree *tree)
{
    int i, j, n0, n1, n10, n01;
    double r;
    
    for (j=0; j<tree->ntaxa; j++) { 
        n0 = n1 = n10 = n01 = 0;
        for (i=0; i<ngenefamily; i++) {
            if (tree->nodes[j].ngenes[i] == 0) {
                n0 += pattern[i];
                if (tree->nodes[j].tumorngenes[i] == 1) {
                    n01 += pattern[i];
                }
            }
            else {
                n1 += pattern[i];
                if (tree->nodes[j].tumorngenes[i] == 0) {
                    n10 += pattern[i];
                }
            }
        }
        r = n0 * n10 / n1 / n01;
        tree->nodes[j].tumorbeta = r;
        tree->nodes[j].brlens_tumor = -log(1-(1+r)*n01/n0)/(1+r);
        //printf("hello %d %d %d %d %d %f %f\n", j, n0, n1, n10, n01, tree->nodes[j].tumorbeta, tree->nodes[j].brlens_tumor);
    }   
    return(NO_ERROR);
}

double conditionProb (int inode, int c, Tree *tree, int igene)
{
	double prob=0.0;
	int son1, son2, i, j;

	if(inode < tree->ntaxa)
	{
		if(tree->nodes[inode].ngenes[igene] == c) prob = 1.0;
		else prob = 0.0;

		//if(tree->nodes[inode].ngenes[igene] == c) prob = LikelihoodBD (c, tree->nodes[inode].tumorngenes[igene], tree->nodes[inode].tumorbeta, tree->nodes[inode].brlens_tumor); //probability of extension branch
		//else prob = 0.0;
	}
	else
	{
		son1 = tree->nodes[inode].sons[0];
		son2 = tree->nodes[inode].sons[1];
		for(i=0; i<2; i++)
		{
			for(j=0; j<2; j++)
			{
				prob += LikelihoodBD (c, i, tree->nodes[son1].beta, tree->nodes[son1].brlens) * conditionProb(son1, i, tree, igene) * LikelihoodBD (c, j, tree->nodes[son2].beta, tree->nodes[son2].brlens) * conditionProb(son2, j, tree, igene);
			}
		}
	}
	return (prob);
}

int Loglike1tree (Tree *tree, int igene, double *loglike)
{
	double prob=0.0;

	//duplication at root has equilibrium distribution
	prob +=	(1-tree->nodes[tree->root].beta) * conditionProb(tree->root, 0, tree, igene);
	prob +=	(tree->nodes[tree->root].beta) * conditionProb(tree->root, 1, tree, igene);
	
	if(prob <= 0.0 || prob > 1.0)
	{
 		printf("probability <= 0");
		return (ERROR);
	}
	*loglike = log(prob);

	return (NO_ERROR);
}

int Loglike (Tree *tree, double *loglike)
{
	int i;
	double x;
	
	*loglike = 0.0;
	for(i=0; i<ngenefamily; i++)
	{
		if(Loglike1tree (tree, i, &x) == ERROR)
		{
			printf("ERROR in LOGLIKE1TREE\n");
			return(ERROR);
		}
		*loglike += pattern[i]*x;
	}
	
	return (NO_ERROR);
}
  
double LikelihoodBD (int s, int c, double beta, double brlens)
{
	double likelihood = 0.0;

	if(s==0 && c==0)
	{
		likelihood = 1-beta*(1-exp(-brlens));
	}
	else if(s == 0 && c == 1)
	{
		likelihood = beta*(1-exp(-brlens));
	}
	else if(s==1 && c==0)
	{
		likelihood = (1-beta)*(1-exp(-brlens));	
	}
	else if(s==1 && c==1)
	{
		likelihood = beta+(1-beta)*exp(-brlens);
	}
	else 
	{
		printf("the numbers must be 0 or 1\n");
		exit(-1);
	}
	return (likelihood);
}
          
void InitialParam (Tree *tree)
{
	int i;

	//initialize beta and branch		
	if(sim == 0)
	{
		for(i=0; i<2*tree->ntaxa-1; i++)
		{
		tree->nodes[i].beta = 0.5;
		tree->nodes[i].maxbeta = 1.0;
		tree->nodes[i].minbeta = 0.0;
		tree->nodes[i].betawindow = 0.01;

		tree->nodes[i].maxbrlens = 5.0;
		tree->nodes[i].minbrlens = 0.0;
		tree->nodes[i].brlenswindow = 0.1;
		}
	}
	
}

int ReadData (FILE *fin)
{
	int i, j, *speciesindex;
	time_t t;
	struct tm *current;
	char datafile[50], outfile[50], simfile[50];
	FILE *fdata; 
	char string[100], skip[100];

	fscanf(fin,"%d%s%ld%ld%d%d", &sim, datafile, &seed, &ngenefamily, &(sptree.ntaxa), &prior_brlens);
		
	if(prior_brlens == 2) fscanf(fin,"%lf", &lambda);
	if(prior_brlens == 3) fscanf(fin,"%lf%lf", &mu, &sigma);
	
    fscanf(fin,"%d", &prior_r);
	if(prior_r == 2) fscanf(fin,"%lf", &lambda_r);
	if(prior_r == 3) fscanf(fin,"%lf%lf", &mu_r, &sigma_r);

	
	if(sim == 0) 
    	{
           	sprintf(outfile, "%s.tre", datafile);
            fout = (FILE*)gfopen(outfile,"w");
            sprintf(outfile, "%s.out", datafile);
            fpredict = (FILE*)gfopen(outfile,"w");
    	}
	else
	{
		sprintf(simfile, "%s", datafile);
		fsim = (FILE*)gfopen(simfile,"w");
	}
	
	/*set seed*/
	if(seed <= 0)
	{
		time(&t);
		current = localtime(&t);
		seed = 123*current->tm_hour + 1111*current->tm_min + 123456*current->tm_sec + 45123;
		SetSeed(seed);
	}
	else	SetSeed(seed);
    
	/*read the species tree*/
	ReadaTree(fin, &sptree);
	for(i=0; i<sptree.ntaxa; i++) //the node sptree.ntaxa stores tumor duplication data
	{
		sptree.nodes[i].ngenes = (int *)SafeMalloc((size_t) (ngenefamily * sizeof(int)));
		if (!sptree.nodes[i].ngenes)
		{
			MrBayesPrint ("%s   Problem allocating sptree.nodes[%d].ngenes (%d)\n", spacer, i,ngenefamily * sizeof(int));
			return (ERROR);
		}
	}

	/*simulation or MCMC*/
	if(sim == 1) 
	{
		fscanf(fin,"%lf", &(sptree.nodes[0].beta));
        if (sptree.nodes[0].beta > 1 || sptree.nodes[0].beta < 0) {
            printf("the parameter beta must be between 0 and 1\n");
            exit(-1);
        }
		for(i=0; i<2*sptree.ntaxa-1; i++)
		{
			sptree.nodes[i].beta = sptree.nodes[0].beta;
		}
		return NO_ERROR;
	}
	else 
	{
		fscanf(fin,"%d%d%d", &mcmc.numGen, &mcmc.sampleFreq, &alphalink);
	}
    
	/*read gene family data*/
	fdata = (FILE*)gfopen(datafile,"r");
	speciesindex = (int *)SafeMalloc((size_t) (sptree.ntaxa * sizeof(int)));
	if (!speciesindex)
	{
		MrBayesPrint ("Problem allocating speciesindex (%d)\n", ngenefamily * sizeof(int));
		return (ERROR);
	}
	for(i=0; i<sptree.ntaxa; i++) speciesindex[i] = -1;
	
	//fscanf(fdata, "%s", skip);
	for(i=0; i<sptree.ntaxa; i++)
	{
		fscanf(fdata,"%s",string);
		for(j=0; j<sptree.ntaxa; j++)
		{
			if(!strcmp(string,sptree.nodes[j].taxaname))
			{
				speciesindex[i] = j;
				break;
			}
		}
		if(speciesindex[i] == -1)
		{
			MrBayesPrint ("CANNOT FIND SPECIES %s in the phylogenetic tree\n",string);
			return (ERROR);
		}
	}
    fscanf(fdata, "%s", skip);
 
	for(i=0; i<ngenefamily; i++)
	{
		for(j=0; j<sptree.ntaxa; j++)
		{
			fscanf(fdata, "%d", &(sptree.nodes[speciesindex[j]].ngenes[i]));
		}
        fscanf(fdata, "%d", &pattern[i]);
	}	
	
	free (speciesindex);
	fclose(fdata);
	return (NO_ERROR);
}

int MoveNode (Tree *tree)
{
	int inode, jnode, father, grandfather, nodes[5], nnodes=0, son;
	double oldloglike, newloglike,diff, random;
    Tree oldtree;
		
	copyTree(tree, &oldtree);
	oldloglike = curLn;
	
	if(rndu() < 0.5)
	{
        do{
			 
            do{
                inode = (int)(rndu() * (2*tree->ntaxa-1));
            }while (inode == tree->root || inode == tree->nodes[tree->root].sons[0] || inode == tree->nodes[tree->root].sons[1]);

            father = tree->nodes[inode].father;
            grandfather = tree->nodes[father].father;
            
            if(tree->nodes[father].sons[0] == inode)
                son = tree->nodes[father].sons[1];
            else
                son = tree->nodes[father].sons[0];
            
            if(son >= tree->ntaxa)
            {
                nodes[nnodes++] = tree->nodes[son].sons[0];
                nodes[nnodes++] = tree->nodes[son].sons[1];
            }		
			
            if(tree->nodes[grandfather].sons[0] == father)
                nodes[nnodes++] = son = tree->nodes[grandfather].sons[1];
            else
                nodes[nnodes++] = son = tree->nodes[grandfather].sons[0];
            
            
            if(son >= tree->ntaxa)
            {
                nodes[nnodes++] = tree->nodes[son].sons[0];
                nodes[nnodes++] = tree->nodes[son].sons[1];
            }
	    }while(nnodes == 0);
        
		jnode = nodes[(int)(rndu() * nnodes)];
        
		swapNodes (tree, inode, jnode);
	}
	else
	{
		do{
			inode = rndu() * tree->ntaxa;
		}while (tree->nodes[inode].father == tree->root);
		do{
			jnode = rndu() * tree->ntaxa;
		}while (jnode == inode);
		rearrangeNodes (tree, inode, jnode);
	}
	
	Loglike (tree, &newloglike);
	
	diff = (newloglike - oldloglike);		
	
	random = log(rndu ());
	if(random > diff) {
        copyTree (&oldtree, tree);
    }
	else curLn += diff;
    
	return (NO_ERROR);
}


int MoveBrlens (Tree *tree)
{
	int nodeindex;
	double max, min, window;
	double oldbrlens, newbrlens, oldloglike, newloglike, diff, random;

	/*pick a node at random*/
	nodeindex = rndu() * (2*tree->ntaxa - 1);
	/*nodeindex = rndu() * (tree->ntaxa + 1);*/

	window = tree->nodes[nodeindex].brlenswindow;
	oldbrlens = tree->nodes[nodeindex].brlens;
	oldloglike = curLn;

	max = Min(tree->nodes[nodeindex].maxbrlens, oldbrlens+window);
	min = Max(tree->nodes[nodeindex].minbrlens, oldbrlens-window);
		
	newbrlens = rndu() * (max-min) + min;
	tree->nodes[nodeindex].brlens = newbrlens;

	Loglike (tree, &newloglike);

	if(prior_brlens == 1) diff = (newloglike - oldloglike);		
	else if(prior_brlens == 2) diff = (newloglike - oldloglike) + (oldbrlens/lambda - newbrlens/lambda);
	else diff = (newloglike - oldloglike) + (0.5*(oldbrlens-mu)*(oldbrlens-mu)/sigma/sigma - 0.5*(newbrlens-mu)*(newbrlens-mu)/sigma/sigma);

	random = log(rndu ());
	if(random > diff) {
        tree->nodes[nodeindex].brlens = oldbrlens;
    }
	else curLn += diff;

	return (NO_ERROR);
}

int MoveBeta (Tree *tree)
{
	int i, nodeindex, index;
	double max, min, window, oldbeta, newbeta, oldloglike, newloglike, diff, random;

	if(alphalink == YES)
	{

        /*save the current values of beta and likelihood*/
		oldbeta = tree->nodes[0].beta;
		oldloglike = curLn;

        /*update beta for all branches*/
        window = tree->nodes[0].betawindow;
		max = Min(tree->nodes[0].maxbeta, oldbeta+window);
		min = Max(tree->nodes[0].minbeta, oldbeta-window);
		newbeta = rndu() * (max-min) + min;
		for(i=0; i<2*(tree->ntaxa)-1; i++) tree->nodes[i].beta = newbeta;

        /*calculate likelihood for the newbeta*/
		Loglike (tree, &newloglike);

        /*calculate hastings ratio*/
		if(prior_r == 1) diff = (newloglike - oldloglike);		
		else if(prior_r == 2) diff = (newloglike - oldloglike) + ( oldbeta/lambda_r - newbeta/lambda_r);
		else diff = (newloglike - oldloglike) + (0.5*(oldbeta-mu_r)*(oldbeta-mu_r)/sigma_r/sigma_r - 0.5*(newbeta-mu_r)*(newbeta-mu_r)/sigma_r/sigma_r);

        /*decide to move or to stay*/
		random = log(rndu ());
		if(random > diff)
		{
			for(i=0; i<2*(tree->ntaxa)-1; i++) tree->nodes[i].beta = oldbeta;
		}
		else curLn += diff;
	}
	else
	{
		/*pick a node at random, a terminal node or a internal node. if internal node, then all internal nodes will be updated with teh same new value of beta*/
		if(rndu() < 0.4) index = 1;
        else index = 2;
        
        if(index == 1) nodeindex = rndu() * (tree->ntaxa);
        if(index == 2) nodeindex = tree->ntaxa + 1;

        /*save the old beta*/
		oldbeta = tree->nodes[nodeindex].beta;
		oldloglike = curLn;
        
        /*update beta*/
        window = tree->nodes[nodeindex].betawindow;
		max = Min(tree->nodes[nodeindex].maxbeta, oldbeta+window);
		min = Max(tree->nodes[nodeindex].minbeta, oldbeta-window);		
		newbeta = rndu() * (max-min) + min;
		if(index == 1) tree->nodes[nodeindex].beta = newbeta;
        if(index == 2) 
        {
            for (i=tree->ntaxa; i<2*tree->ntaxa-1; i++) {
                tree->nodes[i].beta = newbeta;
            }
        }
        
        /*calculate likelihood for the newbeta*/
		Loglike (tree, &newloglike);

        /*calculate hastings ratio*/
		if(prior_r == 1) diff = (newloglike - oldloglike);		
		else if(prior_r == 2) diff = (newloglike - oldloglike) + (oldbeta/lambda_r - newbeta/lambda_r);
		else diff = (newloglike - oldloglike) + (0.5*(oldbeta-mu_r)*(oldbeta-mu_r)/sigma_r/sigma_r - 0.5*(newbeta-mu_r)*(newbeta-mu_r)/sigma_r/sigma_r);
        
        /*decide to move or to stay*/
		random = log(rndu ());
		if(random > diff)
        {
            if(index == 1) tree->nodes[nodeindex].beta = oldbeta;
            if(index == 2) 
            {
                for (i=tree->ntaxa; i<2*tree->ntaxa-1; i++) {
                    tree->nodes[i].beta = oldbeta;
                }
            }
        }
		else curLn += diff;
	}

	return (NO_ERROR);
}

int PrintState (int round, FILE *outfile)
{
	char buffer[30];
  	struct timeval tv;
  	time_t curtime;
	int i, j;
	
	/*print to screen*/
	printf("%s round %d ---loglike: %f\n", spacer, round, curLn);
	
	/*print to file*/
if (updatetree == 0) 
{	
	if(round == 1)
    	{
		gettimeofday(&tv, NULL);
        	curtime = tv.tv_sec;
               	strftime(buffer,30,"%H:%M:%S on %m-%d-%Y",localtime(&curtime));
		fprintf(outfile, "[This analysis was conducted at local time %s with tree ", buffer);
		PrintNodeToFile (outfile, &sptree);
		fprintf(outfile, ". The numbers in the tree are node numbers.]\n");

		fprintf(outfile,"loglike\t");
		if(alphalink == YES)
		{
			for(i=0; i<2*sptree.ntaxa-1; i++)
			{
				fprintf(outfile,"brlens<node%d>\t", i+1);
			}
            
			fprintf(outfile,"beta\t");
            
            for(i=0; i<(int)pow(2,sptree.ntaxa); i++)
			{
                fprintf(outfile,"(");
				for(j=0; j<sptree.ntaxa-1; j++){
                    fprintf(outfile,"%d,", sptree.nodes[j].ngenes[i]);
                }
                fprintf(outfile,"%d)\t", sptree.nodes[sptree.ntaxa-1].ngenes[i]);
			}
		}
		else
		{
			for(i=0; i<2*sptree.ntaxa-1; i++)
			{
				fprintf(outfile,"brlens<node%d>\t", i+1);
                		if(i < sptree.ntaxa+1) fprintf(outfile,"beta<node%d>\t", i+1);
			}
            for(i=0; i<(int)pow(2,sptree.ntaxa); i++)
			{
                fprintf(outfile,"(");
				for(j=0; j<sptree.ntaxa-1; j++){
                    fprintf(outfile,"%d,", sptree.nodes[j].ngenes[i]);
                }
                fprintf(outfile,"%d)\t", sptree.nodes[sptree.ntaxa-1].ngenes[i]);
			}
		}
		fprintf(outfile,"\n");
		fflush(outfile);
	}
	else
	{
		fprintf(outfile,"%lf\t",curLn);
		if(alphalink == YES)
		{
			for(i=0; i<2*sptree.ntaxa-1; i++) fprintf(outfile,"%2.5lf\t",sptree.nodes[i].brlens);
			fprintf(outfile,"%2.5lf\t", sptree.nodes[0].beta);
		}
		else
		{
			for(i=0; i<2*sptree.ntaxa-1; i++)
			{
                fprintf(outfile,"%2.5lf\t",sptree.nodes[i].brlens);
                if(i < sptree.ntaxa+1) fprintf(outfile,"%2.5lf\t", sptree.nodes[i].beta);
			}
		}
		fprintf(outfile,"\n");
		fflush(outfile);
	}
}
	
if (updatetree == 1) 
{
	if(round == 1)
	{
		fprintf(fpredict,"loglike\t\t\tratio_r\n");
		
		gettimeofday(&tv, NULL);
		curtime = tv.tv_sec;
		strftime(buffer,30,"%H:%M:%S on %m-%d-%Y",localtime(&curtime));
		
		fprintf(outfile, "#Nexus\n[This mpest analysis was conducted at local time %s with seed = %ld. The parameters on the extended branches are ", buffer, seed);
        fprintf(outfile, "]\n ");
        
		fprintf(outfile, "Begin trees;\n  translate\n");
		for (i=0; i<sptree.ntaxa-1; i++) {
			fprintf(outfile,"\t%d %s,\n", i+1, sptree.nodes[i].taxaname);
		}
		fprintf(outfile,"\t%d %s;\n", sptree.ntaxa, sptree.nodes[sptree.ntaxa-1].taxaname);
	}
	fprintf(fpredict,"%5.2f\t\t%f\n",curLn, sptree.nodes[0].beta);
	
	if (PrintTree(&sptree, sptree.root, 1, 1, 0, 1) == ERROR)
	{
		printf("Errors in printtree!\n");
		return (ERROR);
	}
	fprintf(outfile, "  tree %d [%2.3f] = %s", round, curLn, printString);
    
    if (round == mcmc.numGen) {
        fprintf(outfile, "End;\n");
    }
	free (printString);
}

	return (NO_ERROR);
}

void PrintHeader (void)
{
	MrBayesPrint ("\n\n\n%s            Bayesian estimation of Cancer evolution  \n\n",spacer);
	srand ((unsigned int)time(NULL));
	MrBayesPrint ("%s                            by\n\n",spacer);
	MrBayesPrint ("%s                        Liang Liu\n\n",spacer);
	MrBayesPrint ("%s                   Department of Statistics\n",spacer);
	MrBayesPrint ("%s                     University of Georgia\n",spacer);
	MrBayesPrint ("%s                       lliu@uga.edu\n\n",spacer);
	MrBayesPrint ("%s        Distributed under the GNU General Public License\n\n",spacer);	
}

/**********************************************************
*
*        simulation
***********************************************************/

int Simulation (Tree *tree)
{
	int i, j, k, l, m, pattern[32][6], index=0;
	double prob[32], sum=0;
	double x;
	
	//fprintf(fsim, "ID");
	for(i=0; i<tree->ntaxa; i++) fprintf(fsim,"%s\t", tree->nodes[i].taxaname);
	fprintf(fsim, "number\n");
	
	for (i=0; i<2; i++) {
		for (j=0; j<2; j++) {
			for (k=0; k<2; k++) {
				for (l=0; l<2; l++) {
					for (m=0; m<2; m++) {
						pattern[index][0] = i;
						pattern[index][1] = j;
						pattern[index][2] = k;
						pattern[index][3] = l;
						pattern[index][4] = m;
						
						tree->nodes[0].ngenes[0] = i;
						tree->nodes[1].ngenes[0] = j;
						tree->nodes[2].ngenes[0] = k;
						tree->nodes[3].ngenes[0] = l;
						tree->nodes[4].ngenes[0] = m;
						
						if(Loglike1tree (tree, 0, &x) == ERROR)
						{
							printf("ERROR in LOGLIKE1TREE\n");
							return(ERROR);
						}
						
						sum += exp(x);
						prob[index] = sum;
						
						printf("%d %d %d %d %d %f\n",pattern[index][0],pattern[index][1],pattern[index][2],pattern[index][3],pattern[index][4], prob[index]);
						index++;
					}
				}
			}
		}
	}
	if (PrintTree(tree, tree->root, 1, 1, 0, 1) == ERROR)
	{
		printf("Errors in printtree!\n");
		return (ERROR);
	}
	printf("%s", printString);
	

	for (i=0; i<32; i++) {
		pattern[i][5] = 0;
	}
	

	for (i=0; i<ngenefamily; i++) {
		x = rndu();
		j = 0;
		while (x > prob[j]) {
			j++;
		}
		pattern[j][5]++;
	}

	for (j=0; j<32; j++) {	
		//fprintf(fsim, "gene%d", j+1);
		for(k=0; k<tree->ntaxa+1; k++)	fprintf(fsim, "%d\t", pattern[j][k]);
		fprintf(fsim, "\n");
	}
	
	return NO_ERROR;
}

