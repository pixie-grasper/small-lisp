10
(cons 10 20)
(cons 10 (cons 20 30))
(car (cons 10 20))
(cdr (cons 10 20))
(atom 10)
(eq 10 20)

(define a 10)
(define b 20)
(cons a b)
(cond ((eq 10 20) 30)
      ((eq 10 30) 40)
      (#t 50))
