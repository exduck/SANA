
#include "Dijkstra.hpp"
#include "../utils/utils.hpp"

using get_time = std::chrono::steady_clock;

Dijkstra::Dijkstra(Graph* G1, Graph* G2, MeasureCombination* MC) :
  Method(G1, G2, "Dijkstra_"+MC->toString()),
  //delta(0.00),
  delta(0.15 + EPSILON), 
  seed_queue(delta, true, G1_exclude, G2_exclude), 
  neighbor_queue(delta, true, G1_exclude, G2_exclude),
  nodes_aligned(0)
{
  this->G1 = G1;
  this->G2 = G2;
  this->MC = MC;

  n1 = G1->getNumNodes();
  n2 = G2->getNumNodes();

  //min of the two is the max nodes
  //that can be aligned
  max_nodes = n1 < n2 ? n1 : n2;
	
  G1->getAdjMatrix(G1AdjMatrix);
  G2->getAdjMatrix(G2AdjMatrix);
  G1->getAdjLists(G1AdjLists);
  G2->getAdjLists(G2AdjLists);

  A = vector<ushort> (n1);

  sims = MC->getAggregatedLocalSims();

}

/* Helper functions */

void Dijkstra::make_seed_queue(){
  std::string fname = G1->getName() + G2->getName() + ".dijkstra";
  std::cout << "graph " << fname << std::endl;
  /*
  if(seed_queue.deserialize(fname)){
    std::cout << "loading from file" << std::endl;
    return;
  }
  */
  std::cout << "make seed queue begin" << std::endl;
  auto start = get_time::now();
  /*
  uint block = 32;
  uint i = 0, j = 0;
  for(i = 0; i < (sims.size() / block) * block; i+=block){
    for(uint r = 0; r < block; ++r){
      for(j = 0; j < (sims[i+r].size() / block) * block; j+=block){
	for(uint k = 0; k < block;++k){
	  seed_queue.insert(sims[i+r][j+k], 
			    std::make_pair(i+r,j+k));
	}
      }      
      for(; j < sims[i].size(); ++j){
	  seed_queue.insert(sims[i+r][j], 
			    std::make_pair(i+r,j));
      }
    }
  }
  
  for(; i < sims.size(); ++i){
    for(j = 0; j < (sims[i].size() / block) * block; j+=block){
      for(uint k = 0; k < block;++k){
	  seed_queue.insert(sims[i][j+k], 
			    std::make_pair(i,j+k));
      }
    }
    for(; j < sims[i].size(); ++j){
	  seed_queue.insert(sims[i][j], 
			    std::make_pair(i,j));
    }
  }
  */
  
  for(unsigned int i = 0; i < sims.size(); ++i){
    for(unsigned int j = 0; j < sims[i].size(); ++j){
      seed_queue.insert(sims[i][j], std::make_pair(i,j));
    }
  }
  
  auto end = get_time::now();
  auto diff = end - start;
  std::cout << "time = (" << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()<< "ms)" << std::endl;
  seed_queue.perf();
  //seed_queue.serialize(fname);
  //std::cout << "serialize done" << std::endl;
}

/*
 * this function should return an ORDERED pair of nodes having
 * the node for G1 in position 0 and 
 * the node for G2 in position 1
 * this function depends on a priority queue
 * to determine the best pair, which can be very slow.
 * future: consider moving the reject loop to the subroutine
 * if the priority queue is empty, it should throw an error
 * This function uses the same exclusion set for all priority queues.
 */

std::pair <ushort, ushort> Dijkstra::best_pair(SkipList & pq) throw(QueueEmptyException){
  //std::cout << "best pair" << std::endl;
  return pq.pop_uniform();
  /*
  std::pair <ushort, ushort> curr_pair;
  do{
    curr_pair = pq.pop_uniform();
    //std::cout << "pop = " << curr_pair.first << ", " << curr_pair.second << std::endl;
    //curr_pair = pq.pop();
  }while(!pq.empty() && (G1_exclude.find(curr_pair.first) != G1_exclude.end() ||
			 G2_exclude.find(curr_pair.second) != G2_exclude.end()) );
  //std::cout << "picked seed (" << curr_pair.first << ", " << curr_pair.second << ")" << std::endl;
  return curr_pair;
  */
}

/* 
 * side effects: this function adds a node to the respective exclusion sets
 * of each graph and adds the pair to the output set.  
 */
void Dijkstra::update_neighbors(std::pair <ushort, ushort> & seed_pair){
  //exclude the seed nodes from future consideration
  G1_exclude.insert(seed_pair.first);
  G2_exclude.insert(seed_pair.second);
  //add the pair to the alignment
  A[seed_pair.first] = seed_pair.second;
  nodes_aligned += 1;
  std::cout << "nodes aligned" << nodes_aligned << std::endl;

  //set difference
  vector<ushort> G1_neighbors = exclude_nodes(G1AdjLists[seed_pair.first], G1_exclude);
  vector<ushort> G2_neighbors = exclude_nodes(G2AdjLists[seed_pair.second], G2_exclude);

  //if there are no neighbors, we can skip the matrix search 
  if(G1_neighbors.empty() || G2_neighbors.empty()){
    return;
  }
	
  //add the possible neighbors to the neighbor_queue
  best_neighbors(G1_neighbors, G2_neighbors);
}

void Dijkstra::best_neighbors(vector<ushort> & G1_neighbors, vector<ushort> & G2_neighbors){
  vector<std::pair<double, std::pair<ushort,ushort>>> out;
  vector<vector<double> > small_matrix (G1_neighbors.size(), vector<double> (G2_neighbors.size()));
	
  /*
    for(std::vector<ushort>::iterator G1_iter = G1_neighbors.begin(); G1_iter != G1_neighbors.end(); ++G1_iter){
    for(std::vector<ushort>::iterator G2_iter = G2_neighbors.begin(); G2_iter != G2_neighbors.end(); ++G2_iter){
    //make a similarity matrix of neighbors
    //small_matrix[*G1_iter][*G2_iter] = sims[*G1_iter][*G2_iter];
    }
    }
  */
  double max_sim = -1;
  for(unsigned int i = 0; i < G1_neighbors.size(); ++i){
    for(unsigned int j = 0; j < G2_neighbors.size(); ++j){
      small_matrix[i][j] = sims[G1_neighbors[i]] [G2_neighbors[j]];
      if(sims[G1_neighbors[i]] [G2_neighbors[j]] > max_sim){
	max_sim = sims[G1_neighbors[i]] [G2_neighbors[j]];
      }
    }
  }
	
  for(unsigned int i = 0; i < G1_neighbors.size(); ++i){
    for(unsigned int j = 0; j < G2_neighbors.size(); ++j){
      //insert pair into priority queue (skip list)
      if(small_matrix[i][j] >= (max_sim - delta)){
	neighbor_queue.insert(small_matrix[i][j], std::make_pair(G1_neighbors[i], G2_neighbors[j]));
      }
    }
  }
}

/*
 * this simulates the set difference of an 
 * adjacency list and an exclusion set. 
 * It returns a vector containing the adjacent nodes 
 * which are not already aligned.  
 */
vector<ushort> Dijkstra::exclude_nodes(vector<ushort> & v_in, std::unordered_set<ushort> & exclusion_set){
  vector<ushort> v_out;
  v_out.reserve(v_in.size()); //minimize reallocations by reserving space in advance
  //|v_out| <= |v_in|
  for(std::vector<ushort>::iterator it = v_in.begin(); it != v_in.end(); ++it){
    if(exclusion_set.find(*it) == exclusion_set.end()){
      v_out.push_back(*it);
    }
  }
  return v_out;
}

/* End Helper Functions */

Alignment Dijkstra::run() {
  // Put all code here
  
  // sims[x][y] will give you how similar node x in G1 is to node y in G2 with 0 being the not at all similar

  // Put alignment into A[x] = y where x is an index of G1 and y is an index of G2

  G1_exclude.clear();
  G2_exclude.clear();

  make_seed_queue();

  std::cout << "Dijkstra begin: max_nodes= " << max_nodes << std::endl;
  while(nodes_aligned < max_nodes){
    auto start = get_time::now();
    try{
      //std::cout << "seed phase" << std::endl;
      //seed phase
      std::pair <ushort, ushort> seed_pair = best_pair(seed_queue);
      update_neighbors(seed_pair);
    }catch (QueueEmptyException & e){
      //can't recover from this
      std::cerr << "Error: no more seeds " << std::endl;
      break;
    }

    auto end = get_time::now();
    auto diff = end - start;
    //std::cout << "seed time(" << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()<< "ms)" << "\t";//<< std::endl;
    //seed_queue.showCounter();

    while(!neighbor_queue.empty()){
      //std::cout << "extend phase" << std::endl;
      start = get_time::now();
      try{
	std::pair <ushort, ushort> neighbor_pair = best_pair(neighbor_queue);
	update_neighbors(neighbor_pair);
      }catch (QueueEmptyException & e){
	//no more neighbors, get another seed
	break;
      }
      //std::cout << "picked neighbor: (" << neighbor_pair.first << ", " << neighbor_pair.second << ")" << std::endl;
      end = get_time::now();
      diff = end - start;
      //std::cout << "(" << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()<< "ms)" << std::endl;
    }

  }
  seed_queue.perf();
  /*
  for(uint i = 0; i < A.size();++i){
    std::cout << i << ": " << A[i] << std::endl;
  }
  */
  return Alignment(A);
}


void Dijkstra::describeParameters(ostream& stream) {
  // Don't worry about this
}

string Dijkstra::fileNameSuffix(const Alignment& A) {
  return "_" + extractDecimals(MC->eval(A),3);
}

Dijkstra::~Dijkstra() {}
