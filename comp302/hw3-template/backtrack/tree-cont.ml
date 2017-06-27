module TreeCont : TREE = 
struct

  type 'a tree = Node of 'a * ('a tree) list 

  exception NotImplemented 

  let rec find_cont p children cont = 
      match children with
            | [] -> cont ()
            | c::children_left -> match c with            
                | Node(x, children_lower) -> if p x then Some(x) 
                   else find_cont p children_lower (fun () -> find_cont p children_left cont)

  let find p t = match t with
      | Node(x , children) -> if p x then Some(x) else  find_cont p children (fun () -> None)
         

end



(*
let t = Node(1,
     [Node(3,[]);
      Node(2, []);
      Node(5,[
          Node(4,[]);
          Node(6, []);
          Node(10,[]);
          Node(20, [])
      ])
 ])

let f a b = if a=b then true else false

*)
