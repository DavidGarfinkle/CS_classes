package student_player.mytools;

import hus.HusMove;

/*
 * Timer Object
 * Keeps track of how much System nano-time is left given a duration
 */
public class HusTimer {

	protected long end_time;
	final long time_limit = 2000000000;
	HusMove failSafe;
	
	public HusTimer(){
		end_time = System.nanoTime() + time_limit;
	}
	
	public boolean outOfTime(){
		if ((end_time - System.nanoTime()) < 0.25e9){
			return true;
		}
		else 
			return false;
	}
	
	public void resetTimer(){
		end_time = System.nanoTime() + time_limit;
	}
	
	public HusMove getFailSafe(){
		return failSafe;
	}
	public void setFailSafe(HusMove m){
		this.failSafe = m;
	}
	public long getTimeLeft(){
		return (this.end_time - System.nanoTime());
	}
	
	
	
}