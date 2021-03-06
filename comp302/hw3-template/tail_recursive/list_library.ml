module List_Library : TAILREC_LIST_LIBRARY = 
  struct
    (* We reimplement some of OCaml's list library functions tail-recursively. As a
     result, these functions will be able to run on large lists and we will not
     get a stack overflow.
     
     *)
    
    exception NotImplemented
		
    (* partition list *)
    (* partition : ('a -> bool) -> 'a list -> 'a list * 'a list
     
     partition p l returns a pair of lists (l1, l2), where l1 is the list of all the elements of l that satisfy the predicate p, and l2 is the list of all the elements of l that donot satisfy p. The order of the elements in the input list is preserved.
     
     *)
    let partition p list = 
        let rec part p left_list (true_list, false_list) = match left_list with
            | []   -> (true_list, false_list)
            | c::l -> if p c then part p l (true_list @ [c], false_list)
                      else part p l (true_list, false_list @ [c])

		in part p list ([],[])		 
  end

(*TEST CODE
#use "sources.ml";;
open List_Library;;
let x = [1;2;3;4;5;6];;
let parity n = (n mod 2 = 0);;
partition parity x;;
*)

(*
#use "sources.ml";;
open List_Library;;
let f x = if x = 2 then true else false;;
partition f [1;2;3;4;5;2;3;2;3;2];;
*)
