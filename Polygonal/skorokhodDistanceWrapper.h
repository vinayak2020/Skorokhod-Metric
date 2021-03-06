/* This is a wrapper for the Skorokhod distance algorithm */
#ifndef _SKOROKHOD_DISTANCE_WRAPPER
#define _SKOROKHOD_DISTANCE_WRAPPER

#include "point.h"
#include "cell.h"
#include "monitor.h"
#include <vector>
#include <utility>

using namespace std;

// class snapshot {
// public:
//     double time;
//     vector<double> pts1;
//     vector<double> pts2;
//     double result;
//     snapshot(double _time,
//               vector<double> _pts1,
//               vector<double> _pts2,
//               double _result) {
//         time = _time;
//         pts1 = _pts1;
//         pts2 = _pts2;
//         result = _result;
//     }
// };



class SkorokhodDistanceWrapper {
public:
    uint16 window;
    double delta;    
    vector<double> scaleVector;
    timePoint *gdata_x;
    timePoint *gdata_y;
    int gcurr_index;
    std::pair<int,int> gprev_curr;
    Cell * (grows[2]);
    Cell * (gcols[2]);
    
    SkorokhodDistanceWrapper(uint16 _window,
            double _delta,
            vector<double> _scaleVector) {
        window = _window;
        delta = _delta;
        timeScale = _timeScale;          
        scaleVector = _scaleVector;
        
        /* From Vinayak's global_defs.cpp*/
        gcurr_index = 0;
        gprev_curr.first = 1;
        gprev_curr.second = 0;
        global_init();
    }
    
    void global_init() {
        for (int i = 0; i<2; i++)
            grows[i] = new Cell[window+1];
        for (int i = 0; i<2; i++)
            gcols[i] = new Cell[window+1];
        gdata_x = new timePoint[window+1];
        gdata_y = new timePoint[window+1];
    }
    
    void global_restart() {
        gcurr_index = 0;
        gprev_curr.first = 1;
        gprev_curr.second = 0;
    }
    
    ~SkorokhodDistanceWrapper() {
        for (int i=0;i<2;i++)
            delete [] grows[i];
        for (int i=0;i<2;i++)
            delete [] gcols[i];
        delete [] gdata_x;
        delete [] gdata_y;
    }
    
    std::pair<bool, bool> scaleAndMonitor(const timePoint & xval,
                                          const timePoint & yval) {
        std::vector<double>::const_iterator iter = scale_vector.begin();        
        timePoint x, y;
        x.time = xval.time*(*iter);
        y.time = yval.time*(*iter);
        x.point.clear(); 
        y.point.clear();        
        ++iter;
        transform(xval.point.begin(), xval.point.end(), iter,
                  x.point.begin(), std::multiplies<double>());
        transform(yval.point.begin(), yval.point.end(), iter,
                  y.point.begin(), std::multiplies<double>());
        return monitor(x,y);
   }
   
   std::pair<bool, bool> monitor(const timePoint &  xval, const timePoint &  yval) {
        //Require gcurr_index \geq 0
        if (gcurr_index == 0){
            gdata_x[0] = xval;
            gdata_y[0] = yval;
            gcurr_index++;            
            return make_pair(true, true);
        }
        
        //now we have  gcurr_index \geq 1
        const int pindx = pIndex_fun(gcurr_index); 
        //Guaranteed to return in range 0..W  if gcurr_index \geq 0
        const int prev_pindx = pIndex_fun(gcurr_index-1);
        
        const int curr_rowcol  = gprev_curr.second;
        const int prev_rowcol  = gprev_curr.first;
        
        gdata_x[pindx] = xval;
        gdata_y[pindx] = yval;
        
        //cout <<"Prev_rowcol:  " <<prev_rowcol <<endl;
        //   return make_pair(true, true);
        double  emin = grows[prev_rowcol][WINDOW-1].getEdgeMin(2) ;
        double emax = grows[prev_rowcol][WINDOW-1].getEdgeMax(2) ;
        
        
        //make top row
        fill_cell_arr(gcurr_index, grows[curr_rowcol], gdata_x, gdata_y[prev_pindx], yval, make_pair(emin, emax), true);
        
        //make last col
        emin = gcols[prev_rowcol][WINDOW-1].getEdgeMin(1) ;
        emax = gcols[prev_rowcol][WINDOW-1].getEdgeMax(1) ;
        fill_cell_arr(gcurr_index, gcols[curr_rowcol], gdata_y, gdata_x[prev_pindx], xval, make_pair(emin, emax), false);
        
        if (gcurr_index == 1) {
            //does the starting point belong to the free space?
            if (grows[curr_rowcol][0].getEdgeMin(0) != 0.0){
                grows[curr_rowcol][0].invalidateCell();
                gcols[curr_rowcol][0].invalidateCell();
                update_gprev_curr(gprev_curr);  //toggle
                gcurr_index++;
                return make_pair(false, false); //further invokations of monitor() will have undefined behavior
            }
            else {
                bool reachable_horizon = edgeValid(grows[curr_rowcol][0], 2) or  edgeValid(gcols[curr_rowcol][0], 1);
                update_gprev_curr(gprev_curr);  //toggle
                gcurr_index++;
                return make_pair(reachable_horizon, true);
            }
        }
        
        //now we have gcurr_index \geq 2
        const int lowest_indx = max(0, gcurr_index -  WINDOW);
        //accessed datapoints from lowest_indx.. gcurr_index. Number of accessed points = gcurr_index - lowest_indx+1
        //This is WINDOW+1 in steady state.
        //number of cells filled (minus triangular cell) = gcurr_index - lowest_indx.
        //filled cells 0... A where A = gcurr_index -lowest_indx - 1
        //and one extra cell in case lowest_indx =  gcurr_index -  WINDOW  and > 0, ie. in steady state
        //need to update from A..0
        int carr_indx =gcurr_index -lowest_indx - 1;
        //gcurr_index -lowest_indx - 1 = gcurr_index - max(0, gcurr_index -  WINDOW) -1
        //geq gcurr_index - 1; and WINDOW -1 \geq 0
        // we have gcurr_index \geq 2
        //the max value is WINDOW -1
        assert( (1<=carr_indx) and (carr_indx < WINDOW));
        
        if (carr_indx <  WINDOW-1 ){//intial special cases            
            grows[curr_rowcol][carr_indx].reachUpdateBot( grows[prev_rowcol][carr_indx-1] );
            gcols[curr_rowcol][carr_indx].reachUpdateLeft( gcols[prev_rowcol][carr_indx-1]);
            carr_indx--;
        }
        
        for(; carr_indx > 0; carr_indx--){            
            grows[curr_rowcol][carr_indx].reachUpdate(
                    grows[curr_rowcol][carr_indx+1], grows[prev_rowcol][carr_indx-1], 0 );
            //carr_indx \leq WINDOW-1 so carr_indx+1 \leq WINDOW
            //carr_indx-1 \geq 0
            gcols[curr_rowcol][carr_indx].reachUpdate(
                    gcols[prev_rowcol][carr_indx-1], gcols[curr_rowcol][carr_indx+1], 0 );
        }
        
        //treat cell 0 as special
        if (gcurr_index -lowest_indx - 1 > 0){
            //if the cell array length was more than 1 (otherwise this case does not apply)
            //should always trigger
            grows[curr_rowcol][0].reachUpdate(grows[curr_rowcol][1], gcols[curr_rowcol][1] , 0); // both [1] elements exist
            gcols[curr_rowcol][0].reachUpdate(grows[curr_rowcol][1], gcols[curr_rowcol][1] , 0); // both [1] elements exist
        }
        
        bool reachable_horizon = false;
        bool reachable_corner = false;
        
        carr_indx =gcurr_index -lowest_indx - 1; //number of cells in cell array, without the triangular cell
        for(; 0<= carr_indx ; carr_indx--){
            reachable_horizon = reachable_horizon or edgeValid(grows[curr_rowcol][carr_indx], 2) or
                    edgeValid(gcols[curr_rowcol][carr_indx], 1);
        }
        reachable_corner = (grows[curr_rowcol][0].getEdgeMax(1) ==1.0 );
        
        
        gcurr_index++;  //latest element has been processed
        update_gprev_curr(gprev_curr);  //toggle
        
        return make_pair(reachable_horizon, reachable_corner);
    }
    
private:
    void update_gprev_curr(pair<int, int> & prev_curr) {
        if(prev_curr.first == 0){
            prev_curr.first = 1;
            prev_curr.second =0;
        }
        else{
            prev_curr.first = 0;
            prev_curr.second =1;
        }
    }
    int pIndex_fun(const int & vindex) {
        // Guaranteed to return in range 0..W  if vindex \geq 0
        // map virtual index to physical index
        return vindex % (WINDOW +1);
    }
            
    bool edgeValid( const Cell & cell, const int & edge) {
        return (cell.getEdgeMin(edge) >= 0.0);
    }      
    
    void fill_cell_arr (const int & gcurr_index, Cell cell_arr[],
            const timePoint tpoints[],
            const timePoint & otherTP1,
            const timePoint & otherTP2,
            const pair<double, double> & e,
            const bool & isRow) {
        // access only gcurr_index - WINDOW, ..., gcurr_index -0 elements from  tpoints[] array.
        // isRow denotes if top row is being filled, or last col
        // Required: cell_arr has size at least WINDOW+1
        // cell_arr[WINDOW] will be a triangular cell based on e, and cell_arr[WINDOW-1]
        
        if (gcurr_index <=0 ) return;
        // now have gcurr_index \geq 1
        Cell tempCell;
        vector<double> tx1(1), tx2(1), ty1(1), ty2(1);
        int  prev_val_indx = gcurr_index -1;
        int next_val_indx = gcurr_index - 0;
        int cell_indx =0;
        const int lowest_indx = max(0, gcurr_index -  WINDOW);        
        //datapoints accessed: lowest_indx, .., gcurr_index
        //in steady state WINDOW+1 datapoints should be accessed        
        const vector<double>   & op1 = otherTP1.point;
        const vector<double>   & op2 = otherTP2.point;
        if(isRow) {
            ty1[0] =  otherTP1.time;
            ty2[0] =  otherTP2.time;
        }
        else{
            tx1[0] =  otherTP1.time;
            tx2[0] =  otherTP2.time;
        }
        
        for(; lowest_indx<= prev_val_indx;
        prev_val_indx--,
                next_val_indx--,
                cell_indx++) {
            // inv: next_val_indx = gcurr_index - cell_indx
            // next_val_indx =  prev_val_indx +1
            // cout<<"In  fill_cell_ar, in for loop" <<endl;
            // indexPrint(cell_indx);
            if (isRow){
                cell_arr[cell_indx].setCorner(gcurr_index -cell_indx-1, gcurr_index - 1);
                //corner = prev_val_indx - cell_indx
                (cell_arr[cell_indx]).freeSpaceFromVec(
                        tpoints[pIndex_fun(prev_val_indx)].point,
                        tpoints[pIndex_fun(next_val_indx)].point,
                        op1, op2, DELTA);
                tx1[0] = tpoints[pIndex_fun(prev_val_indx)].time;
                tx2[0] = tpoints[pIndex_fun(next_val_indx)].time;
                //	cout <<"Temp Cell";
                tempCell.freeSpaceFromVec(tx1, tx2, ty1, ty2, DELTA);
                tempCell.setCorner(gcurr_index -cell_indx-1, gcurr_index - 1);
                //	printCell(tempCell);
                (cell_arr[cell_indx]).intersectWith(tempCell);
            }
            else{
                cell_arr[cell_indx].setCorner(gcurr_index - 1, gcurr_index -cell_indx-1);
                (cell_arr[cell_indx]).freeSpaceFromVec(op1, op2,
                        tpoints[pIndex_fun(prev_val_indx)].point,
                        tpoints[pIndex_fun(next_val_indx)].point,
                        DELTA);
                ty1[0] = tpoints[pIndex_fun(prev_val_indx)].time;
                ty2[0] = tpoints[pIndex_fun(next_val_indx)].time;
                //	cout <<"Temp Cell";
                tempCell.freeSpaceFromVec(tx1, tx2, ty1, ty2, DELTA);
                tempCell.setCorner(gcurr_index -cell_indx-1, gcurr_index - 1);
                (cell_arr[cell_indx]).intersectWith(tempCell);
            }
        }
        // I dont think we need the next 4 comments
        // now next_val_indx = lowest_indx +1
        // cell_indx  equals the 1+number of decrements for   prev_val_indx= gcurr_index -1  to hit  lowest_indx
        // which is at most 1 + +number of decrements  for   prev_val_indx= gcurr_index -1 to hit gcurr_index -  WINDOW
        // which is at most 1 + WINDOW-1 = WINDOW
        if ( (gcurr_index -  WINDOW) > 0){ //need to make last cell triangular in steady state
            //when gcurr_index -  WINDOW = 0, then no triangular cell needed
            if (isRow ){
                cell_arr[WINDOW].setCorner(gcurr_index -WINDOW-1, gcurr_index - 1);
                cell_arr[WINDOW].setEdge(e,  0) ;
                cell_arr[WINDOW].setEdgeMin(cell_arr [WINDOW-1].getEdgeMin(3), 1) ;
                cell_arr[WINDOW].setEdgeMax(cell_arr [WINDOW-1].getEdgeMax(3), 1) ;
                cell_arr[WINDOW].invalidateEdge(  2) ;
                cell_arr[WINDOW].invalidateEdge(  3) ;
            }
            else {
                cell_arr[WINDOW].setCorner(gcurr_index - 1, gcurr_index -WINDOW-1);
                cell_arr[WINDOW].setEdge(e,  3) ;
                cell_arr[WINDOW].setEdgeMin(cell_arr [WINDOW-1].getEdgeMin(0), 2) ;
                cell_arr[WINDOW].setEdgeMax(cell_arr [WINDOW-1].getEdgeMax(0), 2) ;
                cell_arr[WINDOW].invalidateEdge( 0) ;
                cell_arr[WINDOW].invalidateEdge(  1) ;
            }
            //      cell_indx++;
        }
        else{
            //special case in beginning
            //invalidate remaining cells
            for(; cell_indx < WINDOW+1; cell_indx++) {            
                cell_arr[cell_indx].invalidateCell();
                cell_arr[cell_indx].setCorner(- 2, -2); 
            }
        }
    }              
};

#endif
