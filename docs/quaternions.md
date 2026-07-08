# QUATERNIONS

quaternion可以被看作是一个`复数`

An ordered pair of real numbers $z = (a, b)$ is a complex number. 
$a$ is real part and the $b$ is imaginary part. Moreover, equality, addition, subtraction, multiplication and division are defined as follows:

- $(a, b) = (c, d)$ if and only if $a = c$ and $b = d$.
- $(a, b) \pm (c, d) = (a \pm c, b \pm d)$.
- $(a, b)(c, d) = (ac - bd, ad + bc)$.
- $\dfrac{(a,b)}{(c,d)} = \left( \dfrac{ac + bd}{c^2 + d^2},\ \dfrac{bc - ad}{c^2 + d^2} \right)$ if $(c,d) \neq (0,0)$.

The complex `conjugate` of a complex number $z = (a, b)$ is denoted by $\bar{z}$ and given by $\bar{z} = (a,-b)$. A simple way to remember the complex division formula is to multiply the numerator and denominator by the conjugate of the denominator so that the denominator becomes a real number:
$$
\frac{(a,b)}{(c,d)} = \frac{(a,b)(c,-d)}{(c,d)(c,-d)} = \frac{(ac+bd,bc-ad)}{c^2+d^2} = \left(\frac{ac+bd}{c^2+d^2},\frac{bc-ad}{c^2+d^2}\right)
$$

## Geometric Interpretation

<img src="https://img2024.cnblogs.com/blog/2317757/202607/2317757-20260706152734625-2045746541.png"/>

## Quaternion algebra

基本的操作:

An ordered 4-tuple of real numbers $\mathbf{q} = (x, y, z, w) = (q_1, q_2, q_3, q_4)$ is a quaternion. This is commonly abbreviated as $\mathbf{q} = (\mathbf{u}, w) = (x, y, z, w)$, and we call $\mathbf{u} = (x, y, z)$ the imaginary vector part and $w$ the real part. 

Moreover, equality, addition, subtraction, multiplication, and division are defined as follows:


- $(\mathbf{u}, a) = (\mathbf{v}, b)$ if and only if $\mathbf{u} = \mathbf{v}$ and $a = b$.
- $(\mathbf{u}, a) \pm (\mathbf{v}, b) = (\mathbf{u} \pm \mathbf{v}, a \pm b)$.
- $(\mathbf{u}, a)(\mathbf{v}, b) = \bigl(a\mathbf{v} + b\mathbf{u} + \mathbf{u} \times \mathbf{v},\ ab - \mathbf{u} \cdot \mathbf{v}\bigr)$

下面分别定义 $\mathbf{u} \times \mathbf{v}$和 $\mathbf{u} \cdot \mathbf{v}$:

Let $\mathbf{p} = (\mathbf{u}, p_4) = (p_1, p_2, p_3, p_4)$ and $\mathbf{q} = (\mathbf{v}, q_4) = (q_1, q_2, q_3, q_4)$.

- $\mathbf{u} \times \mathbf{v} = (p_2 q_3 - p_3 q_2,\ p_3 q_1 - p_1 q_3,\ p_1 q_2 - p_2 q_1)$
- $\mathbf{u} \cdot \mathbf{v} = p_1 q_1 + p_2 q_2 + p_3 q_3$

这样的话 $pq$变成:

$$
\begin{aligned}
r_1 &= p_4 q_1 + q_4 p_1 + p_2 q_3 - p_3 q_2 = q_1 p_4 - q_2 p_3 + q_3 p_2 + q_4 p_1 \\
r_2 &= p_4 q_2 + q_4 p_2 + p_3 q_1 - p_1 q_3 = q_1 p_3 + q_2 p_4 - q_3 p_1 + q_4 p_2 \\
r_3 &= p_4 q_3 + q_4 p_3 + p_1 q_2 - p_2 q_1 = -q_1 p_2 + q_2 p_1 + q_3 p_4 + q_4 p_3 \\
r_4 &= p_4 q_4 - p_1 q_1 - p_2 q_2 - p_3 q_3 = -q_1 p_1 - q_2 p_2 - q_3 p_3 + q_4 p_4
\end{aligned}
$$

写成矩阵形式:
$$
    \mathbf{pq} =
\begin{bmatrix}
p_4 & -p_3 & p_2 & p_1 \\
p_3 & p_4 & -p_1 & p_2 \\
-p_2 & p_1 & p_4 & p_3 \\
-p_1 & -p_2 & -p_3 & p_4
\end{bmatrix}
\begin{bmatrix}
q_1 \\ q_2 \\ q_3 \\ q_4
\end{bmatrix}
$$

### 属性

不满足`交换律`但是满足`结合律`

In other words, any real number can be thought of as a quaternion with a zero
vector part, and any vector can be thought of as a quaternion with zero real part.
In particular, note that for the identity quaternion, 1  (0, 0, 0, 1). A quaternion
with zero real part is called a pure quaternion

#### Conjugate(共轭) and Norma

The conjugate:

$$
    q^* = -q_{1}-q_{2}-q_{3}+q_{4} = (-u ,q)
$$

#### Inverses

$$
    q^{-1} = \frac{q^*}{\|q\|^2}
$$

The following properties hold for the quaternion inverse:

$$
 (q^{-1})^{-1} = q 
$$
$$
 (pq)^{-1} = q^{-1}p^{-1} 
$$

## Rotation

Let $q = (\mathbf{u}, w)$ be a unit quaternion and let $\mathbf{v}$ be a 3D point or vector. Then we can think of $\mathbf{v}$ as the pure quaternion $p = (\mathbf{v}, 0)$. Also recall that since $q$ is a unit quaternion, we have that $q^{-1} = q^*$. Recall the formula for quaternion multiplication:
\[
(\mathbf{m}, a)(\mathbf{n}, b) = \big(a\mathbf{n} + b\mathbf{m} + \mathbf{m} \times \mathbf{n},\ ab - \mathbf{m} \cdot \mathbf{n}\big)
\]
Now consider the product:
\[
\begin{aligned}
qpq^{-1} &= qpq^* \\
&= (\mathbf{u}, w)(\mathbf{v}, 0)(-\mathbf{u}, w) \\
&= (\mathbf{u}, w)\big(w\mathbf{v} - \mathbf{v} \times \mathbf{u},\ \mathbf{v} \cdot \mathbf{u}\big)
\end{aligned}
\]

最后化简为:

$$
qpq^* = \Big(\big(w^2 - \mathbf{u}\cdot\mathbf{u}\big)\mathbf{v} + 2(\mathbf{u}\cdot\mathbf{v})\mathbf{u} + 2w(\mathbf{u}\times\mathbf{v}),\ 0\Big)
$$

Now, because $q$ is a unit quaternion, it can be written as
$$
q = \big(\sin\theta\,\mathbf{n},\cos\theta\big) \quad \text{for} \quad \|\mathbf{n}\| = 1 \text{ and } \theta \in [0,\pi]
$$

$$
\begin{aligned}
qpq^* &= \big(\cos^2\theta - \sin^2\theta\big)\mathbf{v} + 2\sin^2\theta(\mathbf{n}\cdot\mathbf{v})\mathbf{n} + 2\cos\theta\sin\theta(\mathbf{n}\times\mathbf{v}) \tag{eq. 22.2} \\
&= \cos(2\theta)\mathbf{v} + \big(1 - \cos(2\theta)\big)(\mathbf{n}\cdot\mathbf{v})\mathbf{n} + \sin(2\theta)(\mathbf{n}\times\mathbf{v})
\end{aligned}
$$


这个和书中的矩阵变化公式(eq 3.5)对比,这个是一个旋转矩阵.

Now, compare Equation 22.2 with the axis-angle rotation Equation 3.5 to see that
this is just the rotation formula $R_{\mathbf{n}}(\mathbf{v})$; that is, it rotates the vector (or point) $\mathbf{v}$
about the axis $\mathbf{n}$ by an angle $2\theta$.
$$
R_{\mathbf{n}}(\mathbf{v}) = \cos\theta\,\mathbf{v} + \big(1 - \cos\theta\big)(\mathbf{n}\cdot\mathbf{v})\mathbf{n} + \sin\theta\,(\mathbf{n}\times\mathbf{v})
$$