### **Extrapolation**
Want to extrapolate y_{N+1} given N previous equidistant data points {(1,y1), ..., (N,$y_N$)}.

Because all points are equidistant, can simplify  $\bar{x} \: = \: \dfrac{N+1}{2}$ but still have $\bar{y} \: = \: \dfrac{\sum y_i}{N}$:
### **Standard Ordinary Least Squares**
$\qquad m \: = \: \dfrac{\sum_{i=1}^N (x_i - \bar{x})(y_i - \bar{y})}{\sum_{i=1}^N (x_i - \bar{x})^2}\qquad$
$b \: = \: \bar{y} - m\bar{x}\qquad$
$y=mx+b$

### **Evaluating $y_{N+1}$**
Substitute b into the equation for $y_{N+1}$:
$\\y_{N+1} \: = \: m(x_{N+1}) + b$
$\: = \: m(N+1) + (\bar{y} - m\bar{x})$
$ \: = \: m(N+1 - \bar{x}) + \bar{y}$

Substitute average values: $\quad \bar{x} \: = \: \dfrac{N+1}{2}$ and $\bar{y} \: = \: \dfrac{\sum y_i}{N}$:
$\\y_{N+1} \: = \: m\left(N+1 - \dfrac{N+1}{2}\right) + \dfrac{\sum_{i=1}^N y_i}{N}$
$y_{N+1} \: = \: m\left(\dfrac{N+1}{2}\right) + \dfrac{\sum_{i=1}^N y_i}{N}$

### **Rewriting $m$**
Simplify the denominator of $m$:

$\\ m \: = \: \sum_{i=1}^N (x_i - \bar{x})^2 \: = \: \sum_{i=1}^N x_i^2 - N\bar{x}^2$
$ \: = \: \dfrac{N(N+1)(2N+1)}{6} - N\left(\dfrac{N+1}{2}\right)^2$
$ \: = \: \dfrac{N(N+1)}{2} \left[ \dfrac{2N+1}{3} - \dfrac{N+1}{2} \right]$
$ \: = \: \dfrac{N(N+1)}{2} \left[ \dfrac{2(2N+1) - 3(N+1)}{6} \right]$
$ \: = \: \dfrac{N(N+1)(N-1)}{12} \: = \: \dfrac{N(N^2-1)}{12}$

Simplify the numerator of $m$ (note that $\sum(x_i - \bar{x}) \: = \: 0$):
$\\ \sum_{i=1}^N (x_i - \bar{x})(y_i - \bar{y})$
$ \: = \: \sum_{i=1}^N (x_i - \bar{x}) y_i - \bar{y} \sum_{i=1}^N (x_i - \bar{x})$
$ \: = \: \sum_{i=1}^N (x_i - \bar{x})y_i - 0 $
$ \: = \: \sum_{i=1}^N \left(i - \dfrac{N+1}{2}\right)y_i$

Combine these back to form the simplified $m$:
$$m \: = \: \dfrac{\sum_{i=1}^N \left(i - \dfrac{N+1}{2}\right)y_i}{\dfrac{N(N^2-1)}{12}} \: = \: \dfrac{12 \sum_{i=1}^N \left(i - \dfrac{N+1}{2}\right)y_i}{N(N-1)(N+1)}$$

### **Final Substitution**
Substitute the rewritten $m$ back into our target extrapolation equation for $y_{N+1}$:
$\\y_{N+1} \: = \: \left[ \dfrac{\textcolor{red}{\cancel{12}} \sum_{i=1}^N \left(i - \dfrac{N+1}{2}\right)y_i}{N(N-1)\textcolor{red}{\cancel{(N+1)}}} \right] \left(\dfrac{\textcolor{red}{\cancel{N+1}}}{\textcolor{red}{\cancel{2}}}\right) + \dfrac{\sum_{i=1}^N y_i}{N}$
$ \: = \: \dfrac{6 \sum_{i=1}^N \left(i - \dfrac{N+1}{2}\right)y_i}{N(N-1)} + \dfrac{\sum_{i=1}^N y_i}{N} \;\;(\textcolor{green}{\dfrac{N-1}{N-1}})$
$ \: = \: \dfrac{1}{N(N-1)} \left[ \sum_{i=1}^N 6 \left(i - \dfrac{N+1}{2}\right)y_i + \sum_{i=1}^N (N-1)y_i \right]$
$ \: = \: \dfrac{1}{N(N-1)} \sum_{i=1}^N \left[ 6i - 3(N+1) + N - 1 \right] y_i$
$ \: = \: \dfrac{1}{N(N-1)} \sum_{i=1}^N \left[ 6i - 3N - 3 + N - 1 \right] y_i$
$ \: = \: \dfrac{1}{N(N-1)} \sum_{i=1}^N \left[ 6i - 2N - 4 \right] y_i$
$ \: = \: \dfrac{1}{N(N-1)} \sum_{i=1}^N \left[ (6i) - {(2N + 4)} \right] y_i $
<br><br>This is the result used in the code, where $\textcolor{green}{\text{constants are in green}}$:
$$ y_{N+1} \: = \: \textcolor{green}{\dfrac{1}{N(N-1)}} \sum_{i=1}^N \left[ (6i) - \textcolor{green}{(2N + 4)} \right] y_i $$
