<xsl:stylesheet xmlns:spec="http://www.schemaTest.org/100mb" 
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
                version="1.0">

<xsl:template match="/">
   <html>
      <body>
         <xsl:apply-templates select="spec:site/spec:regions"/>
         <xsl:apply-templates select="spec:site/spec:categories"/>
         <xsl:apply-templates select="spec:site/spec:people"/>
         <xsl:apply-templates select="spec:site/spec:open_auctions"/>
         <xsl:apply-templates select="spec:site/spec:closed_auctions"/>
      </body>
   </html>   
</xsl:template>

<xsl:template match="spec:site/spec:regions">
      <xsl:for-each select="child::*">
         <h3>
            <xsl:text>Country: </xsl:text>
            <xsl:value-of select="name()"/>
         </h3>
         <table border="1">
            <tr>
               <th>Item</th>
               <th>Name</th>
               <th>Location</th>
               <th>Quantity</th>
               <th>Payment</th>
               <th>Description</th>
               <th>Shipping</th>
               <th>Mailing</th>
            </tr>
            <xsl:for-each select="spec:item">
            <xsl:sort order="ascending" select="spec:location"/>
            <tr valign="top">
               <td><xsl:value-of select="./@id"/></td>
               <td><xsl:value-of select="spec:name"/></td>
               <td><xsl:value-of select="spec:location"/></td>
               <td><xsl:apply-templates select="spec:quantity"/></td>
               <td><xsl:value-of select="spec:payment"/></td>
               <td><xsl:apply-templates select="spec:description/spec:parlist"/></td>
               <td><xsl:value-of select="spec:shipping"/></td>
               <td><xsl:apply-templates select="spec:mailbox"/></td>
            </tr>
            </xsl:for-each>
         </table>
      <h3>Stats on this continent</h3>
      <table border="1">
         <tr valign="top">
            <td><xsl:text>Number of nodes in this continent:</xsl:text></td>
            <td><xsl:text>Categories Listed</xsl:text></td>
            <td><xsl:text>Total Quantities</xsl:text></td>
         </tr>
         <tr valign="top">
            <td><xsl:value-of select="count(child::*)"/></td>
            <td>
               <xsl:for-each select="spec:item/spec:incategory">
                  <xsl:sort order="descending" select="./@category"/>
                  <br/>
                  <xsl:number format="(I) " value="(floor(position() div 12)*50)+(position() mod 12 - 1)" />
                  <xsl:value-of select="./@category"/><br/>
               </xsl:for-each>
            </td>
            <td><xsl:value-of select="sum(spec:item/spec:quantity)"/></td>
         </tr>
      </table>
      </xsl:for-each>
</xsl:template>

<xsl:template match="spec:quantity">
   <xsl:value-of select="number()"/>
</xsl:template>

<xsl:template match="spec:mailbox">
   <xsl:for-each select="spec:mail">
   <xsl:sort order="ascending" select="substring(spec:date,7,4)"/>
   <xsl:sort order="ascending" select="substring(spec:date,1,2)"/>
   <xsl:sort order="ascending" select="substring(spec:date,4,2)"/>
   <p><b><xsl:text>To: </xsl:text></b>
      <xsl:call-template name="printEmail">
         <xsl:with-param name="EmailAdd" select="spec:to"/>
      </xsl:call-template><br/>

      <b><xsl:text>From: </xsl:text></b>
      <xsl:call-template name="printEmail">
         <xsl:with-param name="EmailAdd" select="spec:from"/>
      </xsl:call-template><br/>

      <b><xsl:text>Date: </xsl:text></b>
      <xsl:value-of select="spec:date"/><br/>

      <b><xsl:text>Note: </xsl:text></b><br/>
      <xsl:apply-templates select="spec:text"/>
      <xsl:call-template name="FormatText">
         <xsl:with-param name="givenText" select="spec:text"/>
      </xsl:call-template>
   </p>
   </xsl:for-each>
</xsl:template>      

<xsl:template name="printEmail">
   <xsl:param name="EmailAdd"/>
   <xsl:param name="AddrStr" select="substring-after($EmailAdd,'mailto:')"/>
   <xsl:value-of select="substring-before($EmailAdd,'mailto:')"/>
   <a>
      <xsl:attribute name="href">
         <xsl:value-of select="concat('mailto:',$AddrStr)"/>
      </xsl:attribute>
      <xsl:value-of select="substring-after($EmailAdd,'mailto:')"/>
   </a>
</xsl:template>

<xsl:template match="spec:description/spec:parlist">
   <xsl:for-each select="spec:listitem">
      <xsl:number format="(I) " value="(floor(position() div 12)*50)+(position() mod 12-1)" />
      <xsl:apply-templates select="spec:text"/>
      <xsl:call-template name="FormatText">
         <xsl:with-param name="givenText" select="spec:text"/>
      </xsl:call-template>
      <br/>
      <br/>
   </xsl:for-each>            
</xsl:template>

<xsl:template name="FormatText">
   <xsl:param name="givenText"/>
   <p>Encrypted Version</p>
   <xsl:variable name="RevText">	
      <xsl:call-template name="encryptText">
         <xsl:with-param name="normalVerse" select="$givenText"/>
      </xsl:call-template>
   </xsl:variable>
   <xsl:value-of select="$RevText"/>
</xsl:template>

<xsl:template match="spec:text">
   <xsl:apply-templates/>
</xsl:template>

<xsl:template match="spec:bold">
   <b><xsl:apply-templates/></b>
</xsl:template>

<xsl:template match="spec:keyword">
   <i><xsl:apply-templates/></i>
</xsl:template>

<xsl:template match="spec:emph">
   <emph><xsl:apply-templates/></emph>
</xsl:template>

<xsl:template match="spec:site/spec:categories">
         <h3>
            <xsl:text>Category List</xsl:text>
         </h3>
         <table border="1">
            <tr>
               <th>Category #</th>
               <th>Name</th>
               <th>Description</th>
            </tr>
            <xsl:for-each select="spec:category">
            <xsl:sort order="ascending" select="spec:name"/>
            <tr valign="top">
               <td><xsl:value-of select="./@id"/></td>
               <td><xsl:value-of select="spec:name"/></td>
               <td><xsl:apply-templates select="spec:description/spec:parlist"/></td>
            </tr>
            </xsl:for-each>
         </table>

</xsl:template>

<xsl:template match="spec:site/spec:people">
         <h3>
            <xsl:text>Customer Database</xsl:text>
         </h3>
         <table border="1">
            <tr>
               <th>Id</th>
               <th>Name</th>
               <th>Email</th>
               <th>Phone</th>
               <th>homepage</th>
               <th>Credit Card</th>
               <th>Activity</th>
               <th>Mailing</th>
               <th>Income</th>
               <th>Gender</th>
               <th>Business</th>
               <th>Age</th>
               <th>Education></th>
            </tr>
            <xsl:for-each select="spec:person">
            <xsl:sort order="ascending" select="spec:name"/>
            <tr valign="top">
               <td><xsl:value-of select="@id"/></td>
               <td><xsl:value-of select="spec:name"/></td>
               <td><xsl:apply-templates select="spec:emailaddress"/></td>
               <td><xsl:value-of select="spec:phone"/></td>
               <td><xsl:apply-templates select="spec:homepage"/></td>
               <td><xsl:value-of select="spec:creditcard"/></td>
               <td><xsl:apply-templates select="spec:watches"/></td>
               <td><xsl:apply-templates select="spec:address"/></td>
               <td><xsl:apply-templates select="spec:profile/@income"/></td>
               <td><xsl:value-of select="spec:profile/spec:gender"/></td>
               <td><xsl:value-of select="spec:profile/spec:business"/></td>
               <td><xsl:apply-templates select="spec:profile/spec:age"/></td>
               <td><xsl:value-of select="spec:profile/spec:education"/></td>
            </tr>
            </xsl:for-each>   
            <tr>
               <td><b>Totals</b></td>
               <td><b><xsl:value-of select="count(spec:person/spec:name)"/></b></td>            
               <td>--</td>
               <td>--</td>
               <td>--</td>
               <td>--</td>
               <td>--</td>
               <td>--</td>
               <td><b>Ave: <xsl:value-of select="sum(spec:person/spec:profile/@income) div count(spec:person/spec:profile/@income)"/></b></td>
               <td><b><xsl:text># of Male: </xsl:text><xsl:value-of select="count(spec:person/spec:profile[gender='male'])"/><br/>
                   <xsl:text># of Female: </xsl:text><xsl:value-of select="count(spec:person/spec:profile[gender='female'])"/></b>
               </td>
               <td><b><xsl:text># of Bus: </xsl:text><xsl:value-of select="count(spec:person/spec:profile[business='Yes'])"/></b></td>
               <td><b><xsl:text>Ave: </xsl:text><xsl:value-of select="sum(spec:person/spec:profile/spec:age) div count(spec:person/spec:profile/spec:age)"/></b></td>
               <td>--</td>
            </tr>
         </table>

</xsl:template>

<xsl:template match="spec:emailaddress">
   <a>
      <xsl:attribute name="href">
         <xsl:value-of select="."/>
      </xsl:attribute>
      <xsl:value-of select="substring(.,8)"/>
   </a>
</xsl:template>

<xsl:template match="spec:homepage">
   <a>
      <xsl:attribute name="href">
         <xsl:value-of select="."/>
      </xsl:attribute>
      <xsl:value-of select="."/>      
   </a>
</xsl:template>

<xsl:template match="spec:profile/spec:age">
   <xsl:value-of select="number()"/>
</xsl:template>

<xsl:template match="spec:profile/@income">
   <xsl:value-of select="number()"/>
</xsl:template>


<xsl:template match="//spec:address">
   <p><xsl:value-of select="spec:street"/><br/>
      <xsl:value-of select="spec:city"/>
      <xsl:text>, </xsl:text>
      <xsl:value-of select="spec:province"/><br/>
      <xsl:value-of select="spec:country"/><br/>
      <xsl:value-of select="spec:zipcode"/>
   </p>
</xsl:template>

<xsl:template match="//spec:watches">
   <xsl:for-each select="spec:watch">  
      <xsl:number format="(I) " value="(floor(position() div 12)*50)+(position() mod 12-1)" /> 
      <xsl:value-of select="./@open_auction"/><br/>
   </xsl:for-each>  
</xsl:template>

<xsl:template match="spec:site/spec:open_auctions">
   <h2>Auctions Currently Open</h2>
       <xsl:for-each select="spec:open_auction">
         <xsl:sort data-type="number" order="descending" select="current"/>
         <p><b>Item: </b><xsl:value-of select="spec:itemref/@item"/><br/>
            <b>Seller: </b><xsl:value-of select="spec:seller/@person"/><br/>
            <b>Seller Rating: </b><xsl:value-of select="spec:annotation/spec:happiness"/><br/>
            <b>Description: </b><xsl:apply-templates select="spec:annotation/spec:description"/><br/>
            <b>Opening bid: </b><xsl:apply-templates select="spec:initial"/><br/>
            <b>Current bid: </b><xsl:apply-templates select="spec:current"/><br/>
         </p>            
         <h3>
            <xsl:text>Open Auction Activity: </xsl:text>
            <xsl:value-of select="./@id"/>
         </h3>
         <table border="1">
            <tr>
               <th>Date</th>
               <th>Time</th>
               <th>Bidder</th>
               <th>Increment</th>
            </tr>
            <xsl:for-each select="spec:bidder">
            <xsl:sort order="ascending" select="substring(spec:date,7,4)"/>
            <xsl:sort order="ascending" select="substring(spec:date,1,2)"/>
            <xsl:sort order="ascending" select="substring(spec:date,4,2)"/>
            <tr>
               <td><xsl:value-of select="spec:date"/></td>
               <td><xsl:value-of select="spec:time"/></td>
               <td><xsl:value-of select="spec:personref/@person"/></td>
               <td><xsl:apply-templates select="spec:increase"/></td>
            </tr>
            </xsl:for-each>
            <tr>
               <td><b>Total Increase</b></td>
               <td> </td>
               <td> </td>
               <td><xsl:value-of select="sum(spec:bidder/spec:increase)"/></td>
            </tr>
         </table>
      </xsl:for-each>
</xsl:template>

<xsl:template match="spec:annotation/spec:description">
   <xsl:choose>
   <xsl:when test="spec:text">
      <xsl:number format="(I) " value="(floor(position() div 12)*50)+(position() mod 12)" />
      <xsl:apply-templates select="spec:text"/>
      <xsl:call-template name="FormatText">
         <xsl:with-param name="givenText" select="spec:text"/>
      </xsl:call-template>
      <br/>
   </xsl:when>
   <xsl:otherwise>
      <xsl:for-each select="spec:parlist/spec:listitem">
         <xsl:number format="(I) " value="(floor(position() div 12)*50)+(position() mod 12-1)" />
         <xsl:apply-templates select="spec:text"/>
         <xsl:call-template name="FormatText">
            <xsl:with-param name="givenText" select="spec:text"/>
         </xsl:call-template>
         <br/>
         <br/>
      </xsl:for-each>            
   </xsl:otherwise>
   </xsl:choose>
   
</xsl:template>

<xsl:template match="spec:initial">
   <xsl:value-of select="number()"/>
</xsl:template>

<xsl:template match="spec:current">
   <xsl:value-of select="number()"/>
</xsl:template>

<xsl:template match="spec:increase">
   <xsl:value-of select="number()"/>
</xsl:template>

<xsl:template match="spec:site/spec:closed_auctions">
         <h3>
            <xsl:text>Closed Auction</xsl:text>
         </h3>
         <table border="1">
            <tr>
               <th>Buyer</th>
               <th>Seller</th>
               <th>Item #</th>
               <th>Price</th>
               <th>Date</th>
               <th>Quantity</th>
               <th>Prev Bidder</th>
               <th>Description</th>
               <th>Satisfaction</th>
            </tr>
         <xsl:for-each select="spec:closed_auction">
            <xsl:sort order="descending" select="substring(spec:date,7,4)"/>
            <xsl:sort order="descending" select="substring(spec:date,1,2)"/>
            <xsl:sort order="descending" select="substring(spec:date,4,2)"/>
            <tr valign="top">
               <td><xsl:value-of select="spec:buyer/@person"/></td>
               <td><xsl:value-of select="spec:seller/@person"/></td>
               <td><xsl:value-of select="spec:itemref/@item"/></td>
               <td><xsl:apply-templates select="spec:price"/></td>
               <td><xsl:value-of select="spec:date"/></td>
               <td><xsl:apply-templates select="spec:quantity"/></td>
               <td><xsl:value-of select="spec:annotation/spec:author/@person"/></td>
               <td><xsl:apply-templates select="spec:annotation/spec:description"/></td>
               <td><xsl:value-of select="spec:annotation/spec:happiness"/></td>
            </tr>
         </xsl:for-each>
            <tr>
               <td><b>Totals</b></td>
               <td> </td>
               <td> </td>
               <td><b><xsl:value-of select="sum(spec:closed_auction/spec:price)"/></b></td>
               <td> </td>
               <td><b><xsl:value-of select="sum(spec:closed_auction/spec:quantity)"/></b></td>
               <td> </td>
               <td> </td>
               <td>Average: <xsl:value-of select="sum(spec:closed_auction/spec:annotation/spec:happiness) div count(spec:closed_auction/spec:itemref)"/></td>
            </tr>
         </table>
</xsl:template>

<xsl:template match="spec:price">
   <xsl:value-of select="number()"/>
</xsl:template>

<xsl:template name="encryptText">
   <xsl:param name="normalVerse"/>
   <xsl:param name="backwardsText" select="Empty"/>
   <xsl:choose>
      <xsl:when test="string-length($normalVerse) = 0">
         <xsl:value-of select="$backwardsText"/>
      </xsl:when>
      <xsl:otherwise>
         <xsl:call-template name="encryptText">
            <xsl:with-param name="normalVerse" select="substring($normalVerse,2,string-length($normalVerse)-1)"/>
            <xsl:with-param name="backwardsText" select="concat(substring($normalVerse,1,1),$backwardsText)"/>
         </xsl:call-template>
      </xsl:otherwise>
   </xsl:choose>
</xsl:template>

</xsl:stylesheet>