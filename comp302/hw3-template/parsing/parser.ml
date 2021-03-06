module  Parser : PARSER = 
struct

(* In this exercise, you implement a  parser for a
 simple context free grammar using continuations. The grammar parses arithmetic
 expressions with +,*, and ()'s.  The n represents an integer and is a
 terminal symbol. Top-level start symbol for this grammar is E.  

E-Expression: E ::= S;          
S-Expression: S ::= P + S | P   
P-expression: P ::= A * P | A   
A-expression: A ::= n | (S)

How to read the grammar? - There are 4 different kinds of expressions:
E-expresion (top-level expression), 
S-expression (an expression with top-level symbol + ), 
P-expression (an expression with top-level symbol * ), and 
A-expression (an atomic expression, i.e. either a number or an expression in 
              brackets).

More precisely the grammar gives an answer to the following 4 questions: 

What is an E-expression? - It is a S-expression followed by a semicoln.

What is an S-expression? - It is a P-expression followed by "+" and another
   S-expression OR simply a P-expression

What is a P-expression? - It is an A-expression followed by "*" and another
   P-expression OR simply an A-expression.

What is an A-expression? - It is an atomic expression; either it is simply a
   number or it is a S-expression wrapped in brackets.

Note according to this grammar * binds tighter than +, as is also usually the case.

Expressions wil be lexed into a list of tokens of type Token.

For example "9 + 8 * 7;" is turned by the lexer into a list of tokens
[INT(9),PLUS,INT(8),TIMES,INT(7),SEMICOLON].

We then call the parser to translate the list of tokens into an abstract syntax
tree following the rules of the grammar.

   parse [L.INT(9);L.PLUS;L.INT(8);L.TIMES;INT(7);SEMICOLON]
   ===> Sum(Int 9, Prod (Int 8, Int 7))

The principles we use here are similar to the regular expression
matcher discussed in class. As we get the list of tokens we do not know how to
split it such that we can form  a proper S-expression. Instead, we will process the
token list together with a continuation: token list -> exp -> exp.

Part of the token list will be used to build an expression e1. The continuation receives the remaining
token list toklist' for further processing and the expression e1. It will then continue to
parse toklist' and build compound expressions given e1.

For example, to parse a S-expression we use part of the token list to build a P-expression, called e; when we are done 
the continuation receives a remaining token list, called tok_list', together with an expression e. 
Following the grammar rules, if tok_list' contains as a next token PLUS, we continue building an S-expression;
otherwise we simply return e which is also a valid S-expression.

If parsing was successful, the token stream will eventually be empty,
and we can simply return the built expression e.

#use "sources.ml";;
open Parser;;


*)


module L = Lexer

type exp = Sum of exp * exp | Prod of exp * exp | Int of int

exception Error of string

let terminal_f = fun s e -> match s with [] -> e | _ -> raise (Error "Error: Incomplete expression")


let rec parseExp toklist sc = 
	let rec loop left acc = match left with
	    | (L.SEMICOLON)::l ->  parseSum acc sc(*  sc l ( parseSum acc (fun s e -> match s with [] -> e | _ -> Sum(e, parseSum s  terminal_f)) )  *)
        (*| (L.LPAREN)::l -> parseAtom (L.LPAREN)::l  (fun s e -> )    *)
	    | a::l -> loop l (acc@[a])
	    | [] -> raise (Error "Wrong expression syntax")
	in loop toklist []

and parseSum toklist sc = 	
    let rec loop left acc = match left with
	    | (L.PLUS)::l -> parseProd acc (fun s e -> match s with [] -> parseSum l (fun s' e' -> sc s' (Sum(e, e')) )
	    	                                                   | _ -> raise (Error "Error: Sum_1"))
	    (*let e = parseProd acc  (fun s e -> match s with [] -> e | _ -> Prod(e, parseProd s terminal_f ))  
	                             in sc l e *)
	    (*| (L.LPAREN)::l -> if acc = [] then parseProd ((L.LPAREN)::l) (fun s e -> parseSum s (fun s' e' -> sc) )
	                                   else parseProd acc (fun s e -> match s with [] -> parseProd ((L.LPAREN)::l)  (fun s' e' -> sc s' (Prod(e, e')) )
	                                                                               | _ -> raise (Error "Error: Sum_2") )
        *)

	    | a::l -> loop l (acc@[a])
	    | []   ->  parseProd toklist ( fun s e -> match s with [] -> sc [] e | _ -> raise (Error "Sum_3") )(*sc [] (parseProd acc terminal_f)*)
	in loop toklist []

and parseProd toklist sc = 	
    let rec loop left acc = match left with
	    | (L.TIMES)::l -> parseAtom acc (fun s e -> match s with [] -> parseProd l (fun s' e' -> sc s' (Prod(e, e')) ) 
	    	                                                    | _ ->  raise (Error "Error: Prod_1"))
	    (*| (L.LPAREN)::l -> if acc = [] then parseAtom ((L.LPAREN)::l) (fun s e -> match s with [] -> sc s e 
	                                                                                           | _ -> )
	                                   else parseProd acc ( fun s e -> match s with [] ->  parseProd ((L.LPAREN)::l) (fun s' e' -> sc s' (Prod(e,  e') ) ) 
	                                                                               | _ -> raise (Error "Error: Prod_2")   )
        *)

	    (*let e = parseProd acc  (fun s e -> match s with [] -> e | _ -> Prod(e, parseProd s terminal_f ))  
	                             in sc l e *)
	    | a::l -> loop l (acc@[a])
	    | []   ->  parseAtom acc (fun s e -> match s with [] -> sc [] e | _ -> raise (Error "Prod_3"))
	in loop toklist []


and parseAtom toklist sc = match toklist with 
        | L.INT(a)::l -> sc [] (Int(a))
        (*
        | (L.LPAREN)::l -> let rec loop left acc = match left with
                | (L.RPAREN)::l -> parseSum acc ( fun s e -> match s with [] -> sc l e | _ ->  raise (Error "Error: Atom_1") )
                | x::l -> loop l (acc@[x])
                | _ -> (raise (Error "Error: Atom_2 - Parent Related"))
               in loop l [] 
        *)
        | _ -> raise (Error "Error: Atom_3 - Parent Related")

let parse string  = parseExp (L.lex string) (fun s e -> match s with [] -> e | _ -> raise (Error "Error: Incomplete expression"))

end






(*


module L = Lexer

type exp = Sum of exp * exp | Prod of exp * exp | Int of int

exception Error of string

let terminal_f = fun s e -> match s with [] -> e | _ -> raise (Error "Error: Incomplete expression")


let rec parseExp toklist sc = 
	let rec loop left acc = match left with
	    | (L.SEMICOLON)::l -> sc l ( parseSum acc (fun s e -> match s with [] -> e | _ -> Sum(e, parseSum s  terminal_f)) )
	    | a::l -> loop l (a::acc)
	    | [] -> raise (Error "Wrong expression syntax")
	in loop toklist []

and parseSum toklist sc = 	
    let rec loop left acc = match left with
	    | (L.PLUS)::l -> parseProd acc (fun s e -> match s with [] -> sc l e | _ -> sc l (Sum(e, (parseSum s sc) ))  )
	    (*let e = parseProd acc  (fun s e -> match s with [] -> e | _ -> Prod(e, parseProd s terminal_f ))  
	                             in sc l e *)
	    | a::l -> loop l (a::acc)
	    | []   ->  sc [] (parseProd acc terminal_f)
	in loop toklist []

and parseProd toklist sc = 	
    let rec loop left acc = match left with
	    | (L.TIMES)::l -> parseAtom acc (fun s e -> match s with [] -> sc l e | _ -> sc l (Prod(e, (parseProd s sc) ))  )
	    (*let e = parseProd acc  (fun s e -> match s with [] -> e | _ -> Prod(e, parseProd s terminal_f ))  
	                             in sc l e *)
	    | a::l -> loop l (a::acc)
	    | []   ->  sc [] (parseAtom acc terminal_f)
	in loop toklist []


and parseAtom toklist sc = match toklist with
    | L.INT(a)::l -> sc l (Int(a))
    | x::l -> sc l (Int(0))
    | [] -> sc [] (Int(0))

let parse string  = parseExp (L.lex string) (fun s e -> match s with [] -> e | _ -> raise (Error "Error: Incomplete expression"))

end


*)
