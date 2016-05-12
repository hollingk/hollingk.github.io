/*
  Author: Brian Roark
  Date: 9 March 2005
  
  Updated by Kristy Hollingshead
  31 January 2007
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#define MAXLABLEN 80920
#define MAXNUMWDS 70000
#define WHVAL 2015179

#define USAGE "\n***Usage***: %s [-opts] -i file  \n\
Options:                                          \n\
 -i          training data file (required)        \n\
 -e          test data file     (optional)        \n\
 -f [w,p,t]  filetype [word, pos, tree]           \n\
 -h          display this help message            \n"

// these need to be defined here
// so we can reference struct pointers within structs
struct word_info;
typedef struct word_info WordInfo;
struct word_hash;
typedef struct word_hash WordHash;
struct node;
typedef struct node Node;

struct word_info
{
  char* label;
  int idx;
  float count;
};

// a simple structure to access the hash table
struct word_hash
{
  WordInfo* word;
  WordHash* next;
};

struct node
{
  int idxlabel;
  int idxword;
  Node* firstchild;
  Node* lastchild;
  Node* parent;
  Node* leftsib;
  Node* rightsib;
};

void failproc(char* msg)
{
  fprintf(stderr,"%s\n",msg);
  exit(1);
}

// generate a hash code from a string key (label)
int get_whash(char* label)
{
  int i, hash, len=strlen(label), p=1987, w, lw=label[0];
  long lhash=0;
  for (i=0; i < len; i++) {
    w=label[i];
    lhash += w*lw*p;
    lw=w;
    p-=4;
    if (p <= 0) p=1987;
    lhash=lhash%WHVAL;
  }
  hash=lhash%WHVAL;
  return hash;
}

// search for a word-object in the hash table.
// returns null if word is not found in table.
WordInfo* find_word(char* word, WordHash** hashtable, int *hash)
{
  WordHash* whash;
  (*hash)=get_whash(word);
  whash=hashtable[(*hash)];
  while (whash != NULL) {
    if (strcmp(word,whash->word->label) == 0) return whash->word;
    whash=whash->next;
  }
  return NULL;
}

// read a word from the input file (readfile),
// reading up until the delimiter character (delimit),
// find the word in the hash table.
// increments location (i) in readfile.
// returns null if word is not found in table.
WordInfo* get_find_word(char* readfile, char* readtok, int *i, int *hash, WordHash** hashtable, char delimit)
{
  int j=0;
  while (readfile[(*i)] != delimit && readfile[(*i)]!='\n')
    readtok[j++]=readfile[(*i)++];
  readtok[j]=0;
  return find_word(readtok,hashtable,hash);
}

// add a word to the hash table.
// returns the new word-object.
WordInfo* insert_word(char* lbl, int idx, WordHash** hashtable, int hash)
{
  WordHash *whash=hashtable[hash], *newhash=NULL;
  // whash points to the first entry in the chain of entries
  // reference by this hash code
  WordInfo* wentry=malloc(sizeof(WordInfo));
  wentry->label=lbl;
  wentry->idx=idx;
  wentry->count=0.0;
  newhash=malloc(sizeof(WordHash));
  newhash->word=wentry;
  newhash->next=whash;
  // now newhash->next points to the first entry in the chain
  hashtable[hash]=newhash;
  // inserts newhash at the beginning of the chain
  return wentry; // return the word-object
}

// add a word to the word array.
// causes the word to also be added to the hash table.
WordInfo* add_word(char* readtok, int *widx, int *hash, WordHash** hashtable, WordInfo** wordlist)
{
  WordInfo *word=NULL;
  char* lbl=malloc((strlen(readtok)+1)*sizeof(char));
  strcpy(lbl,readtok);
  word=wordlist[(*widx)]=insert_word(lbl,*widx,hashtable,(*hash));
  (*widx)++;
  assert((*widx)<MAXNUMWDS); // make sure there's room in the wordlist
  return word;
}

// initialize a node;
// (stores indices into wordlist instead of char*s.)
Node* init_node(int label, int word)
{
  Node *node=malloc(sizeof(Node));
  node->idxlabel=label;
  node->idxword=word;
  node->firstchild=NULL;
  node->lastchild=NULL;
  node->parent=NULL;
  node->leftsib=NULL;
  node->rightsib=NULL;
  return node;
}

void free_node(Node *node)
{
  if (node==NULL) return;
  Node *child=node->firstchild, *tempchild;
  while (child != NULL) {
    tempchild=child->rightsib;
    free_node(child);
    child=tempchild;
  }
  free(node);
}

void print_tree(FILE *fp, Node *node, WordInfo** wordlist)
{
  if (node==NULL) return;
  if (node->idxlabel==-1) {
    // a list -- just print words and rightsibs,
    //           no pos-labels and no children
    Node *sib=NULL;
    fprintf(fp, "%s", wordlist[node->idxword]->label);
    sib=node->rightsib;
    while (sib) {
      fprintf(fp, " %s", wordlist[sib->idxword]->label);
      sib=sib->rightsib;
    }
    fprintf(fp, "\n"); // done with this list
    return;
  }
  fprintf(fp, "(%s ", wordlist[node->idxlabel]->label);
  if (node->idxword!=-1) fprintf(fp, "%s", wordlist[node->idxword]->label);
  print_tree(fp, node->firstchild, wordlist);
  fprintf(fp, ")"); // done with this node
  if (node->rightsib!=NULL) {
    fprintf(fp, " ");
    print_tree(fp, node->rightsib, wordlist);
  }
  if (node->parent==NULL && node->leftsib==NULL)
    fprintf(fp, "\n"); // done with this tree
}

// creates a list of nodes from a text file containing word strings.
// returns the 'head' node.
Node* read_words(char* readfile, char* readtok, int *linepos, WordHash** hashtable, int* wcount, WordInfo** wordlist)
{
  int hash;
  Node *node=NULL, *fnode=NULL, *prevnode=NULL;
  WordInfo *word;
  while (readfile[(*linepos)] != '\n') {
    while (readfile[(*linepos)]==' ') (*linepos)++;
    word=get_find_word(readfile,readtok,linepos,&hash,hashtable,' ');
    if (word==NULL)
      word=add_word(readtok,wcount,&hash,hashtable,wordlist);
    prevnode=node;
    node=init_node(-1,word->idx);
    node->leftsib=prevnode;
    if (fnode==NULL) fnode=node;
    if (prevnode!=NULL) prevnode->rightsib=node;
    while (readfile[(*linepos)]==' ') (*linepos)++;
  }
  (*linepos++); // final '\n'
  return fnode;
}

// creates a list of nodes from a text file containing
//   '(POS word)' sequences.
// returns the 'head' node.
Node* read_poswords(char* readfile, char* readtok, int *linepos, WordHash** hashtable, int *wcount, WordInfo** wordlist)
{
  int hash;
  Node *node=NULL, *fnode=NULL, *prevnode=NULL;
  WordInfo *word, *pos;
  while (readfile[(*linepos)] != '\n') {
    while (readfile[(*linepos)]==' ') (*linepos)++;
    if (readfile[(*linepos)] != '(')
      failproc("oops, misparse: missing beginning bracket");
      // POS sequences must start with an open bracket
    (*linepos)++;
    pos=get_find_word(readfile,readtok,linepos,&hash,hashtable,' ');
    if (pos==NULL) pos=add_word(readtok,wcount,&hash,hashtable,wordlist);
    while (readfile[(*linepos)]==' ') (*linepos)++;
    word=get_find_word(readfile,readtok,linepos,&hash,hashtable,')');
    if (word==NULL)
      word=add_word(readtok,wcount,&hash,hashtable,wordlist);
    prevnode=node;
    node=init_node(pos->idx,word->idx);
    node->leftsib=prevnode;
    if (fnode==NULL) fnode=node;
    if (prevnode!=NULL) prevnode->rightsib=node;
    while (readfile[(*linepos)]==')') (*linepos)++;
    while (readfile[(*linepos)]==' ') (*linepos)++;
  }
  (*linepos++); // final '\n'
  return fnode;
}

// creates a tree (tree-structured lists of nodes) from a text file
//   containing trees.
// returns the 'root' node.
Node* read_tree(char* readfile, char* readtok, int *linepos, Node* parent, Node* lsib, WordHash** hashtable, int *wcount, WordInfo** wordlist)
{
  int hash;
  Node *node=NULL, *prevc=NULL, *child=NULL;
  WordInfo *word, *nt;
  if (readfile[(*linepos)] != '(')
    failproc("oops, misparse: missing beginning bracket");
    // trees must start with an open bracket
  (*linepos)++;
  // read in text until next ' ' character is read: node label (POS)
  nt=get_find_word(readfile,readtok,linepos,&hash,hashtable,' ');
  if (nt==NULL) nt=add_word(readtok,wcount,&hash,hashtable,wordlist);
  node=init_node(nt->idx,-1);
  node->parent=parent;
  if (parent) parent->lastchild=node;
  node->leftsib=lsib;

  // read the rest of the "node"; check for leaf status
  while (readfile[(*linepos)] != ')') {
    while (readfile[(*linepos)]==' ') (*linepos)++;
    if (readfile[(*linepos)]!='(') {
      // if next character is *not* a '(', then this is a leaf
      word=get_find_word(readfile,readtok,linepos,&hash,hashtable,')');
      if (word==NULL)
        word=add_word(readtok,wcount,&hash,hashtable,wordlist);
      node->idxword=word->idx;
    }
    else {
      // recurse
      child=read_tree(readfile,readtok,linepos,node,prevc,hashtable,wcount,wordlist);
      if (prevc==NULL) node->firstchild=child;
      else prevc->rightsib=child;
      prevc=child;
    }
  }
  (*linepos)++;
  return node;
}

int main(int ac, char* av[])
{
  int c, err=0, id=0, linepos, wcount=1;
  char readtok[MAXLABLEN], readfile[MAXLABLEN], filetype='t';
  FILE *trainfp=NULL, *testfp=NULL;
  WordInfo** wordlist=malloc(MAXNUMWDS*sizeof(WordInfo*));
  WordHash** hashtable;
  Node* tree; // root node of a structure of nodes
  extern char *optarg;
  extern int optind;

  while ((c = getopt(ac, av, "i:e:f:h?")) != -1) // add more options here
      switch (c) {
          case 'i':
            trainfp=fopen(optarg, "r");
            if (trainfp==NULL) {
                fprintf(stderr, "couldn't open file %s\n", optarg);
                exit(1);
            }
            break;
          case 'e':
            testfp=fopen(optarg, "r");
            if (testfp==NULL) {
                fprintf(stderr, "couldn't open file %s\n", optarg);
                exit(1);
            }
            break;
          case 'f':
            filetype=optarg[0];
            // w for word strings,
            // p for (POS word) sequences,
            // t for trees
            break;
          case 'h':
          case '?':
          default:
              err++;
      }

  if ( err || trainfp==NULL || ac < optind ) {
      fprintf(stderr, USAGE, av[0]);
      exit(1);
  }

  // initialize word hash
  hashtable=calloc(WHVAL,sizeof(WordHash*));
  /*** do any other initializations you need here ***/

  // read in training file(s)
  // note: filenames don't need to be passed in as args,
  //       can also hard-code them in like this:
  //       trainfp=fopen("f2-21.training.txt","r");
  while (fgets(readfile,MAXLABLEN,trainfp) != NULL) {
    id++;
    linepos=0;

    // read in one tree (or one line of the file)
    // reads every label and word into the hash table and wordlist
    // note: filetype doesn't need to be passed in as an arg,
    //       can also hard-code it in like this:
    //       filetype='w';
    switch (filetype) {
      case 'w':
        tree=read_words(&readfile[0],&readtok[0],&linepos,hashtable,&wcount,wordlist);
        break;
      case 'p':
        tree=read_poswords(&readfile[0],&readtok[0],&linepos,hashtable,&wcount,wordlist);
        break;
      case 't':
        tree=read_tree(&readfile[0],&readtok[0],&linepos,NULL,NULL,hashtable,&wcount,wordlist);
        break;
    }

    /*** do any calculations, updates, etc.
         that you might need with this tree ***/

    // for debugging purposes...
    fprintf(stderr, "Here's the training text we just read in: ");
    print_tree(stderr, tree, wordlist);

    free_node(tree);
    if (id%1000==0) fprintf(stderr,"%d ",id);
    else if (id%10 == 0) fprintf(stderr,".");
  }
  fprintf(stderr,"%d\n",id);

  // read in test file, if provided as arg
  // note: filenames don't need to be passed in as args,
  //       can also hard-code them in like this:
  //       testfp=fopen("f2-21.testing.txt","r");
  if (testfp) {
    id=0;
    while (fgets(readfile,MAXLABLEN,testfp) != NULL) {
      id++;
      linepos=0;

      /*** do any initializations you need here ***/

      // read in one tree (or one line of the file)
      // reads every label and word into the hash table and wordlist
      // note: filetype doesn't need to be passed in as an arg,
      //       can also hard-code it in like this:
      //       filetype='w';
      switch (filetype) { // assumes same filetype as training file...
        case 'w':
          tree=read_words(&readfile[0],&readtok[0],&linepos,hashtable,&wcount,wordlist);
          break;
        case 'p':
          tree=read_poswords(&readfile[0],&readtok[0],&linepos,hashtable,&wcount,wordlist);
          break;
        case 't':
          tree=read_tree(&readfile[0],&readtok[0],&linepos,NULL,NULL,hashtable,&wcount,wordlist);
          break;
      }

      /*** apply your learned model to this tree ***/

      // for debugging purposes...
      fprintf(stderr, "Here's the test text we just read in: ");
      print_tree(stderr, tree, wordlist);

      free_node(tree);
      if (id%100 == 0) fprintf(stderr," %d ",id);
      else if (id%10 == 0) fprintf(stderr,".");
    }
    fprintf(stderr,"%d\n",id);
  }

  return EXIT_SUCCESS;
}
